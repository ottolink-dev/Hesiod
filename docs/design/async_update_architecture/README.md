# Asynchronous Graph Update Architecture

This document describes the multi-tier threading model, dependency-driven DAG scheduling, and synchronization mechanisms used during asynchronous graph updates in Hesiod and GNode.

## Overview

The graph update architecture separates user interactions on the GUI thread from heavy computational node processing in the background. It distributes execution across three distinct tiers:

1. **GUI Thread (Qt Event Loop)**: Coordinates graph updates, freezes node states (`begin_background_compute()`) before starting asynchronous passes, and handles completion callbacks without blocking the UI.
2. **Pass Coordinator Thread**: A dedicated background thread managed by `hesiod::GraphWorker` that orchestrates a single update pass.
3. **Task Worker Pool (`gnode::ThreadPool`)**: A pool of worker threads executing independent DAG nodes concurrently as their upstream dependencies complete.

## Architecture & Thread Tiers

The following diagram illustrates how the tiers communicate:

![Thread Architecture](diagrams/thread_architecture.svg)

```mermaid
graph TB
    subgraph GUI_Tier ["GUI Thread (Qt Event Loop)"]
        UI["User Interactions / Parameter Edits"]
        GW["GraphWorker"]
        Snap["Snapshot Creation<br/><code>begin_background_compute()</code>"]
        PostRecv["Completion Handler<br/><code>end_pass()</code>"]
    end

    subgraph Worker_Tier ["GraphWorker Background Thread"]
        CoordThread["Pass Coordinator Thread<br/>(1 per active pass)"]
        GNodeCoord["<code>Graph::execute_parallel()</code><br/>DAG In-Degree Orchestrator"]
    end

    subgraph Pool_Tier ["GNode ThreadPool (Worker Pool)"]
        T1["Pool Worker 1"]
        T2["Pool Worker 2"]
        Tn["Pool Worker N"]
    end

    UI -->|triggers| GW
    GW -->|1. Snapshots planned nodes| Snap
    GW -->|2. Spawns std::thread| CoordThread
    CoordThread -->|3. Calls| GNodeCoord
    GNodeCoord -->|4. Enqueues in-degree 0 tasks| T1
    GNodeCoord -->|4. Enqueues in-degree 0 tasks| T2
    GNodeCoord -->|4. Enqueues in-degree 0 tasks| Tn
    CoordThread -.->|Blocks on sync_cv| GNodeCoord
    CoordThread -->|5. Posts end_pass to event loop| PostRecv
```

Source: [thread_architecture.mmd](diagrams/thread_architecture.mmd)

## Sequence and Synchronization Flow

The end-to-end execution of a pass proceeds as follows:

1. When an update is triggered, `GraphWorker::start_next()` snapshots all dirty nodes upfront on the GUI thread by invoking `BaseNode::begin_background_compute()`.
2. A coordinator thread is spawned to execute `gnode::Graph::execute_parallel()`.
3. In-degree counts are computed for all dirty nodes in the scheduled subgraph. Nodes with in-degree 0 are dispatched immediately to the `ThreadPool`.
4. As each node finishes, it atomically decrements the in-degree of its downstream neighbors. When a neighbor's in-degree reaches 0, it is enqueued to the pool.
5. The coordinator waits on a `std::condition_variable` until all dirty nodes finish or cancellation is signaled.

![Sequence Flow](diagrams/sequence_flow.svg)

```mermaid
sequenceDiagram
    autonumber
    participant GUI as GUI Thread
    participant WT as Worker Thread (GraphWorker)
    participant TP as ThreadPool Workers (GNode)

    Note over GUI: User edits parameter or requests update
    GUI->>GUI: GraphWorker::start_next()
    GUI->>GUI: Snapshot all planned nodes: begin_background_compute()
    GUI->>WT: Spawn std::thread(graph->execute_parallel)

    Note over WT: Graph::execute_parallel()
    WT->>WT: Calculate in-degree for dirty subgraph
    WT->>WT: Create sync_mutex, sync_cv, cancelled=false
    WT->>WT: Create ThreadPool
    WT->>TP: enqueue(initial nodes with in_degree == 0)

    par Parallel Task Execution
        TP->>TP: Check cancel_update_callback(node_id)
        alt is cancelled or pass mismatched
            TP->>WT: cancelled = true, sync_cv.notify_all()
        else proceed with node compute
            TP->>TP: Node::update() / compute() using frozen snapshots
            critical Atomic in-degree decrement
                TP->>TP: For each downstream node:<br/>in_degree.fetch_sub(1) == 1?
            end
            opt If downstream in_degree reaches 0
                TP->>TP: enqueue(downstream_node)
            end
            critical Atomic remaining decrement
                TP->>WT: if (--remaining_nodes == 0)<br/>lock(sync_mutex) & sync_cv.notify_all()
            end
        end
    and Coordinator Wait
        WT->>WT: sync_cv.wait(lock, remaining_nodes == 0 || cancelled)
    end

    Note over WT: Unblocked from wait
    WT->>WT: ~ThreadPool() joins all pool workers safely
    WT->>GUI: post(end_pass, start_next)
    GUI->>GUI: end_pass(): cancel unstarted snapshots, emit update_finished
```

Source: [sequence_flow.mmd](diagrams/sequence_flow.mmd)

## Locking and Synchronization Primitives

Thread safety and inter-thread coordination are achieved using dedicated primitives:

![Synchronization Primitives](diagrams/synchronization_primitives.svg)

```mermaid
classDiagram
    class GraphWorker_Primitives {
        +std::atomic~bool~ cancelled
        +std::atomic~int~ pass
        +std::mutex started_mutex
        +std::set~string~ started
        -- Purpose --
        cancelled: Signals background threads to stop early without blocking GUI
        pass: Discards obsolete callbacks & results if graph was modified
        started_mutex: Protects the set of nodes that began execution across pool threads
    }

    class GNode_ExecuteParallel_Primitives {
        +std::atomic~int~ remaining_nodes
        +std::atomic~bool~ cancelled
        +std::mutex sync_mutex
        +std::condition_variable sync_cv
        +std::mutex error_mutex
        +std::exception_ptr captured_error
        -- Purpose --
        remaining_nodes: Counts down dirty nodes left to execute
        sync_mutex + sync_cv: Blocks coordinator thread until remaining_nodes == 0 or cancelled
        error_mutex + captured_error: Captures first thrown exception thread-safely
    }

    class GNode_DAG_Dependency_Primitives {
        +std::atomic~int~ in_degree_per_node
        +ThreadPool::queue_mutex
        +ThreadPool::cv
        -- Purpose --
        in_degree: Lock-free atomic count of unresolved upstream dirty parents
        queue_mutex: Protects ThreadPool task queue when scheduling ready nodes
    }

    GraphWorker_Primitives <.. GNode_ExecuteParallel_Primitives : cancel_update_callback bridges them
    GNode_ExecuteParallel_Primitives *-- GNode_DAG_Dependency_Primitives : coordinates scheduling
```

Source: [synchronization_primitives.mmd](diagrams/synchronization_primitives.mmd)

## Cancellation and Exception Safety

To avoid deadlocks and use-after-scope errors during early cancellation or node exceptions:

1. **Stack Lifetime Ordering**: In `gnode::Graph::execute_parallel()`, synchronization primitives (`sync_mutex`, `sync_cv`, `cancelled`, `remaining_nodes`, and captured error state) are declared before `ThreadPool pool`. When exiting scope, `~ThreadPool()` joins all pool threads before any synchronization state is unwound.
2. **Exception Handling**: Node execution is wrapped in a `try ... catch (...)` block. If an exception occurs, the first thrown exception is stored in `captured_error`, `cancelled` is set to `true`, `remaining_nodes` is decremented, and `sync_cv` is notified to wake the coordinator immediately without hanging.
3. **Early Cancellation**: Cancellation callbacks verify whether the pass is still valid or has been cancelled by the GUI before starting a node's computation.

![Cancellation and Exceptions](diagrams/cancellation_and_exceptions.svg)

```mermaid
flowchart TD
    Start(["Node threw exception OR cancel requested"]) --> Decision{"Source of event?"}

    Decision -->|Exception during Node::update| CatchBlock["catch (...) in Pool Worker"]
    CatchBlock --> LockErr["lock(error_mutex)"]
    LockErr --> StoreErr["captured_error = std::current_exception()"]
    StoreErr --> SetCancel["cancelled.store(true)"]
    SetCancel --> DecrRem["--remaining_nodes"]
    DecrRem --> WakeCV["lock(sync_mutex) & sync_cv.notify_all()"]

    Decision -->|GUI calls cancel or stop| SetGWCancel["GraphWorker::cancelled.store(true)"]
    SetGWCancel --> CheckCb["Worker checks cancel_update_callback(node_id)"]
    CheckCb --> SetGNodeCancel["cancelled.store(true)"]
    SetGNodeCancel --> WakeCV

    WakeCV --> UnblockCoordinator["sync_cv.wait() unblocks in Coordinator"]
    UnblockCoordinator --> DrainPool["~ThreadPool() destructor joins pool workers<br/><i>(Safe: stack primitives declared before pool)</i>"]
    DrainPool --> RethrowCheck{"captured_error != nullptr?"}
    RethrowCheck -->|Yes| Rethrow["rethrow_exception(captured_error)"]
    RethrowCheck -->|No| CleanReturn["Return to Worker Thread -> GUI end_pass()"]
```

Source: [cancellation_and_exceptions.mmd](diagrams/cancellation_and_exceptions.mmd)
