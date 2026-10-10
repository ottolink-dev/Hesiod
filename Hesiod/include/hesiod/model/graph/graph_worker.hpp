/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace hesiod
{

class GraphNode; // forward

// =====================================
// GraphWorker
// =====================================

// Updates the graphs on a thread of its own, one graph at a time, so that the
// interface stays usable meanwhile.
//
// The update itself is gnode::Graph::update(). Everything else happens on the
// GUI thread: the requests, the edits of the model and what follows an update.
// Before each node the update thread asks the GUI thread to go on (through
// gnode::Graph::cancel_update_callback), which is where an update is cancelled,
// and where the node gets ready: a snapshot of its settings to compute from, its
// data kept from being read until it is done (see BaseNode).
class GraphWorker
{
public:
  ~GraphWorker();

  // Queues these nodes and what is downstream of them, or the whole graph, and
  // cancels the update of that graph in progress: it resumes with what it had
  // left plus these nodes. Never waits. False when the caller has to update the
  // graph itself.
  bool request(GraphNode &graph, const std::vector<std::string> *p_node_ids = nullptr);

  // Cancels the update in progress and waits for the node being computed: the
  // graphs can then be modified. What is left resumes from the event loop.
  void stop();

  // Drops what is queued for this graph, or for all of them, and stops.
  void cancel(const GraphNode *p_graph = nullptr);

  // Stops, then computes what is queued before returning.
  void finish();

  // Runs fct on the GUI thread, from the event loop.
  void post(std::function<void()> fct);

  static bool in_update_thread();

  // Set once by the application: off without an event loop (CLI modes).
  static inline std::function<bool()>                      enabled;
  static inline std::function<void(std::function<void()>)> post_to_gui; // event loop
  static inline std::function<void()> thread_setup; // on the update thread

  std::function<void(std::exception_ptr)> failed; // an update was abandoned

private:
  struct Job
  {
    std::weak_ptr<GraphNode> graph = {};
    std::set<std::string>    node_ids = {};
    bool                     all = false;
  };

  bool                      ask(int pass, const std::string &node_id); // cancel?
  void                      answer(int pass, const std::string &node_id);
  void                      end_pass(); // joins the update thread and takes stock
  std::deque<Job>::iterator find_job(const GraphNode &graph);
  void                      schedule_start_next();
  void                      start_next(); // unless an update is in progress

  // GUI thread
  std::deque<Job>            jobs;    // one per graph, in request order
  std::shared_ptr<GraphNode> graph;   // being updated
  std::vector<std::string>   planned; // its nodes, in update order
  std::set<std::string>      started; // those the update thread could compute
  std::exception_ptr         error;   // what the update thread ended with
  std::thread                thread;
  int                        pass = 0; // identifies the update in progress
  bool                       start_next_pending = false;

  // shared with the update thread
  std::mutex              mutex;
  std::condition_variable cv;
  bool                    cancelled = false;
  bool                    go = false;

  std::shared_ptr<int> alive = std::make_shared<int>(); // for what is posted
};

} // namespace hesiod
