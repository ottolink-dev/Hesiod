/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <utility>

#include "hesiod/model/graph/graph_node.hpp"
#include "hesiod/model/graph/graph_worker.hpp"
#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

namespace
{
thread_local bool is_update_thread = false;
} // namespace

GraphWorker::~GraphWorker()
{
  // returns once the node being computed, if any, is done
  {
    std::lock_guard<std::mutex> lock(this->mutex);
    this->cancelled = true;
  }
  this->cv.notify_one();

  if (this->thread.joinable())
    this->thread.join();
}

void GraphWorker::answer(int pass, const std::string &node_id)
{
  std::lock_guard<std::mutex> lock(this->mutex);

  if (pass != this->pass || !this->graph || this->cancelled)
    return;

  if (auto *p_node = this->graph->get_node_ref_by_id<BaseNode>(node_id))
    p_node->begin_background_compute();

  this->started.insert(node_id);
  this->go = true;
  this->cv.notify_one();
}

bool GraphWorker::ask(int pass, const std::string &node_id)
{
  std::unique_lock<std::mutex> lock(this->mutex);

  this->go = false;
  if (!this->cancelled)
    this->post([this, pass, node_id]() { this->answer(pass, node_id); });

  // a node that got ready is computed, a cancellation then stops at the next one
  this->cv.wait(lock, [this]() { return this->go || this->cancelled; });
  return !this->go;
}

void GraphWorker::cancel(const GraphNode *p_graph)
{
  this->stop();
  std::erase_if(this->jobs,
                [p_graph](const Job &job)
                { return !p_graph || job.graph.lock().get() == p_graph; });
}

void GraphWorker::end_pass()
{
  if (!this->thread.joinable())
    return;

  this->thread.join();
  ++this->pass; // what it has posted is outdated

  auto graph = std::exchange(this->graph, nullptr);
  graph->cancel_update_callback = nullptr;

  std::set<std::string> left;
  for (const auto &node_id : this->planned)
    if (!this->started.contains(node_id))
      left.insert(node_id);

  auto job = this->find_job(*graph);

  if (this->error)
  {
    // as for a blocking update, what is left is not worth computing
    this->jobs.clear();
    if (this->failed)
      this->failed(std::exchange(this->error, nullptr));
  }
  else if (!left.empty() || job != this->jobs.end())
  {
    // what was left goes first, with what has been requested meanwhile
    Job resumed = job != this->jobs.end() ? std::move(*job) : Job{.graph = graph};
    if (job != this->jobs.end())
      this->jobs.erase(job);

    resumed.node_ids.insert(left.begin(), left.end());
    this->jobs.push_front(std::move(resumed));
  }
  else if (graph->update_finished)
    graph->update_finished();

  std::lock_guard<std::mutex> lock(this->mutex);
  this->cancelled = false;
}

void GraphWorker::finish()
{
  this->stop();

  // what is left, on this thread
  while (!this->jobs.empty())
  {
    Job job = std::move(this->jobs.front());
    this->jobs.pop_front();

    auto graph = job.graph.lock();
    if (!graph)
      continue;

    std::vector<std::string> ids;
    for (const auto &node_id : job.node_ids)
      if (graph->get_node(node_id))
        ids.push_back(node_id);

    if (job.all)
      graph->gnode::Graph::update();
    else if (!ids.empty())
      graph->gnode::Graph::update(ids);

    if (graph->update_finished)
      graph->update_finished();
  }
}

bool GraphWorker::in_update_thread() { return is_update_thread; }

std::deque<GraphWorker::Job>::iterator GraphWorker::find_job(const GraphNode &graph)
{
  return std::ranges::find_if(this->jobs,
                              [&graph](const Job &job)
                              { return job.graph.lock().get() == &graph; });
}

void GraphWorker::post(std::function<void()> fct)
{
  if (this->post_to_gui)
    this->post_to_gui(
        [alive = std::weak_ptr<int>(this->alive), fct = std::move(fct)]()
        {
          if (alive.lock())
            fct();
        });
}

bool GraphWorker::request(GraphNode &graph, const std::vector<std::string> *p_node_ids)
{
  if (!this->enabled || !this->enabled() || !this->post_to_gui)
  {
    this->finish();
    return false;
  }

  auto job = this->find_job(graph);
  if (job == this->jobs.end())
    job = this->jobs.insert(job, Job{.graph = graph.weak_from_this()});

  if (p_node_ids)
    job->node_ids.insert(p_node_ids->begin(), p_node_ids->end());
  else
    job->all = true;

  // what the update in progress computes from now on is outdated
  if (this->graph.get() == &graph)
  {
    std::lock_guard<std::mutex> lock(this->mutex);
    this->cancelled = true;
    this->cv.notify_one();
  }

  // not before the caller is done with the graphs
  this->post([this]() { this->start_next(); });
  return true;
}

void GraphWorker::start_next()
{
  while (!this->thread.joinable() && !this->jobs.empty())
  {
    Job job = std::move(this->jobs.front());
    this->jobs.pop_front();

    auto graph = job.graph.lock();
    if (!graph)
      continue;

    std::vector<std::string> ids;
    for (const auto &[node_id, _] : graph->get_nodes())
      if (job.all || job.node_ids.contains(node_id))
        ids.push_back(node_id);

    if (ids.empty())
      continue;

    this->graph = graph;
    this->planned = job.all ? graph->topological_sort(ids)
                            : graph->get_nodes_to_update(ids);
    this->started.clear();
    this->error = nullptr;
    this->go = false;
    const int pass = ++this->pass;

    graph->cancel_update_callback = [this, pass](const std::string &node_id)
    { return this->ask(pass, node_id); };

    this->thread = std::thread(
        [this, graph, ids, all = job.all, pass]()
        {
          is_update_thread = true;
          if (this->thread_setup)
            this->thread_setup();

          try
          {
            if (all)
              graph->gnode::Graph::update();
            else
              graph->gnode::Graph::update(ids);
          }
          catch (...)
          {
            this->error = std::current_exception();
          }

          this->post(
              [this, pass]()
              {
                if (pass != this->pass)
                  return;
                this->end_pass();
                this->start_next();
              });
        });
  }
}

void GraphWorker::stop()
{
  if (!this->thread.joinable())
    return;

  {
    std::lock_guard<std::mutex> lock(this->mutex);
    this->cancelled = true;
  }
  this->cv.notify_one();
  this->end_pass();

  // what is left resumes once the caller is done with the graphs
  this->post([this]() { this->start_next(); });
}

} // namespace hesiod
