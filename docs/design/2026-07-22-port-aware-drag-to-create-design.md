# Port-Aware Drag-to-Create — Design

**Date:** 2026-07-22
**Status:** Approved (design), pending implementation plan
**Scope:** Hesiod GUI — the drag-from-port → create-node → auto-connect interaction

## 1. Problem

Dragging from a node port onto empty canvas opens the node-creation menu; after a node is
created, Hesiod tries to auto-connect it. Two defects make this abort the application.

**Crash A (observed).** Drag `VirtualArray` from `Bump.output`, drop on canvas, create an
`IslandChain`. The auto-connect loop in `GraphNodeWidget::on_connection_dropped` picks the first
port whose `data_type` matches — **without checking the port's direction** — and lands on
`IslandChain.out`, an *output*. `gnode::Graph::new_link`
(`external/GNode/GNode/src/graph.cpp:397`) validates direction and throws
`std::invalid_argument: Port 'out' on node '9' must be an input port.` The exception escapes a Qt
slot, so `std::terminate` aborts the process.

The loop's own comment says *"connect to the first **input** that has the same type"* — the
direction filter was simply never written.

**Crash B (latent, same family).** Connection drags are not direction-guarded:
`GraphicsNode::mousePressEvent` starts a drag from any hovered port, input or output. The
auto-connect always passes the dragged port as `from`, so dragging *backwards* from an input and
creating a node throws `must be an output port` — the mirror-image abort.

**Secondary defect.** `add_link` (GUI) is called *before* `new_link` (model), so a rejected link
can leave a phantom edge drawn on screen.

**Provenance.** `on_connection_dropped` was last modified by `78e0b781` *("Add missing model
creation on connection dropped event (fix #611)")*. This is upstream behaviour and is **not**
related to the Meta attribute migration; it reproduces on `dev`.

## 2. Goals

1. The creation menu shown during a drag lists **only node types that can actually connect** to
   the dragged port.
2. Auto-connect afterwards is **deterministic and documented**, in both drag directions.
3. Neither `new_link` direction check can abort the application.

### Non-goals

- Making the node library panel, search, or plain right-click-add context-aware. Those keep their
  current behaviour; only the drag path filters.
- Changing type compatibility rules, port declarations, or any node's ports.
- Refactoring node port declaration into a static registry.

## 3. Approach

Port knowledge must exist *before* any node is created, so it cannot come from a live node.
`AppContext::load_node_documentation` already loads `node_documentation.json` at startup, and that
file carries, for every port of every node type, both `data_type` and `type` (`input`/`output`):

```json
"IslandChain": { "ports": {
    "out":  { "data_type": "VirtualArray", "type": "output" },
    "path": { "data_type": "Path",         "type": "input"  } }}
```

Coverage was verified against the node inventory: **302 / 302 node types have a docs entry and
port data; none are missing.** So the filter reads data already in memory — no extra I/O, no node
instantiation, no startup cost.

**Rejected alternatives.** Building the catalog by instantiating all ~302 node types at startup is
authoritative but pays a full construction sweep (several nodes allocate arrays/GPU resources)
every launch for data available for free. A declarative port registry would remove the
generated-artifact dependency permanently but touches ~300 node files for a menu filter.

The generated-artifact risk (a node added without regenerating docs) is handled by **failing
open**: an unknown node type stays visible.

## 4. Components

The decision logic is kept **pure and Qt-free** — the rules that can be wrong become testable
without a GUI, which is precisely why the current bug survived.

| Unit | Responsibility | Depends on |
|---|---|---|
| `PortCatalog` | `node_type → [{name, data_type, direction}]`, built once from loaded documentation | node documentation JSON |
| `is_offerable` (pure fn) | Does this node *type* have any port of the wanted direction and type? | `PortCatalog` |
| `select_port` (pure fn) | Given a **live node's** ports, which one do we connect? | — |
| menu filter hook | `execute_new_node_context_menu()` accepts an **optional** predicate `bool(node_type)` | `is_offerable` |
| `on_connection_dropped` | Qt only: resolve dragged port, open filtered menu, create link | the above |

**Two questions, two data sources — deliberately.**

- *"Can this node type connect at all?"* is asked **before** any node exists, so it must use the
  docs catalog. It only needs a port to **exist**, so ordering is irrelevant.
  `is_offerable(node_type, data_type, wanted_direction) → bool`; an unknown type returns `true`
  (fail-open).
- *"Which port do we connect?"* is asked **after** the node has been created, so it queries the
  **live node** (`get_nports`/`get_port_label`/`get_port_type`/`get_data_type`, already exposed).
  `select_port(live_node, data_type, wanted_direction) → optional<port_name>`.

This split matters: the catalog stores ports as a JSON object whose key order is **alphabetical,
not declaration order** (Bump appears as `control, dx, dy, envelope`). Selecting from the catalog
would therefore pick an arbitrary port. The live node preserves true declaration order, so
"first declared" is only meaningful — and is only used — on the live node.

Calling the menu with **no** predicate must behave exactly as today, so non-drag creation paths
are untouched.

## 5. Behaviour contract

**Offer rule.** A node type is offered iff it has ≥1 port whose direction is the **opposite** of
the dragged port's and whose `data_type` matches **exactly**. Exact string match is deliberate:
`VirtualArray` and `VirtualTexture` are not interchangeable, and this mirrors what the link layer
enforces, so the menu cannot offer something that would then be rejected.

**Symmetry.** One rule, operands swapped:

| Dragged from | Menu offers types with | Link created |
|---|---|---|
| an **output** | a matching **input** | `dragged.out → new.in` |
| an **input** | a matching **output** | `new.out → dragged.in` |

`from` is always resolved to the OUT side before calling `new_link`, so both direction checks
become **unreachable** rather than merely guarded.

**Port selection when several match.** Evaluated against the **live node**, so "first declared"
means true declaration order. Prefer a conventionally-named port for the direction sought —
`input`/`in` when seeking an input, `output`/`out` when seeking an output (case-insensitive) —
otherwise the **first declared**. Consequences, accepted:

- `Laplace` has `input` → connects `input`.
- `Bump` declares `dx, dy, control, envelope`, none conventional → connects **`dx`** (first
  declared). Deterministic and documented; the user rewires when a different input was intended.
  Note this is `dx` only because selection reads the live node; the docs catalog would have
  yielded `control` (alphabetical), which is why selection never uses the catalog.

**Empty filtered set.** If no node type accepts the dragged type at all, show the **full,
unfiltered list**. A gesture that opens an empty or absent menu reads as broken; a permissive menu
still lets the node be placed, and it simply will not auto-connect. This relaxes the
"only compatible are visible" promise only in the case where that promise is vacuous.

**No compatible port after creation** (reachable only via a fail-open unknown type): the node is
created, no link is made, logged at trace. Not an error.

## 6. Error handling

1. Operand ordering makes both `new_link` direction throws unreachable. The link call is
   additionally wrapped so no exception can escape a Qt slot into `std::terminate`.
2. **Model link first, GUI link second** — the GUI edge is drawn only if the model link succeeded,
   eliminating phantom edges.
3. Unknown node type → offered (fail-open); if no port is found afterwards, no link, logged.
4. Menu cancelled → nothing created, nothing linked (existing return flag already covers this).

## 7. Verification

Hesiod has **no C++ unit-test harness** (`tests/` is Python, covering the hsd toolkit and doc
generation, and is not wired into CMake). The established idiom for headless C++ verification in
this codebase is a CLI subcommand — `--parity-dump`, `--compat-check`. This design follows it.

**`--check-port-links`** instantiates each node type (as `--parity-dump` already does) and sweeps
**all 302 node types × every distinct data type × both directions**, asserting two invariants:

> **Safety.** If a candidate is offered, `select_port` on the created node returns a port of the
> required direction — i.e. a valid link is always constructible, so `new_link` can never throw.

> **Agreement.** For every node type, `is_offerable` (docs catalog) matches what the live node
> actually has. This turns the design's one residual risk — docs drift — into a failing check
> rather than a silently mis-filtered menu.

Non-zero exit on any violation. It also pins the specific cases:

| Case | Expected |
|---|---|
| IslandChain, `VirtualArray` dragged from an output | **not offered** (crash A) |
| IslandChain, `VirtualArray` dragged from an input | offered → `out` (crash B path) |
| IslandChain, `Path` dragged from an output | offered → `path` |
| Laplace, `VirtualArray` dragged from an output | offered → `input` (conventional name) |
| Bump, `VirtualArray` dragged from an output | offered → `dx` (first declared, live-node order) |
| unknown node type | offered (fail-open), `select_port` → none |

Because the subcommand calls the same pure functions the GUI calls, it cannot drift from shipped
behaviour.

**Manual GUI verification** (user-driven) covers the gesture itself: the menu is visibly filtered,
the link is drawn, both drag directions work, and creating an unconnected node still works.

## 8. Accepted trade-offs

- Dropping onto a multi-input node (e.g. `Bump`) connects the first declared input, which may not
  be the intended one. Chosen over an ambiguity picker to keep the gesture fast; manual rewiring
  is acceptable. A picker remains a possible later refinement.
- The catalog depends on a generated artifact. Mitigated three ways: coverage is currently 100%,
  unknown types fail open (never hidden), and the `--check-port-links` agreement invariant turns
  any future drift into a failing check.
- Exact `data_type` matching may exclude a pairing that a future node intends to allow; that would
  be a deliberate change to the type rules, out of scope here.
