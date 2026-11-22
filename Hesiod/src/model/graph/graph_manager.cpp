/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <new>
#include <string>

#include "nlohmann/json.hpp"

#include "gnode/graph_worker.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/graph/graph_config.hpp"
#include "hesiod/model/graph/graph_manager.hpp"
#include "hesiod/model/graph/graph_node.hpp"
#include "hesiod/model/utils.hpp"
#include "hesiod/profiling.hpp"

namespace hesiod
{

GraphManager::GraphManager(const std::string &id)
    : gnode::Graph(id), worker(std::make_unique<gnode::GraphWorker>())
{
  Logger::log()->trace("GraphManager::GraphManager: id: {}", id);

  this->worker->failed = [this](std::exception_ptr error)
  { this->on_update_failed(error); };

  this->worker->held_changed = [this](bool held)
  {
    if (this->update_held)
      this->update_held(held);

    for (auto &[_, p_node] : this->nodes)
      if (auto *graph = dynamic_cast<GraphNode *>(p_node.get()))
        if (graph->update_held)
          graph->update_held(held);
  };
}

GraphManager::~GraphManager() = default;

std::shared_ptr<GraphManager> GraphManager::get_shared()
{
  try
  {
    return std::dynamic_pointer_cast<GraphManager>(gnode::Graph::shared_from_this());
  }
  catch (...)
  {
    Logger::log()->critical(
        "GraphManager::get_shared: object is not managed by shared_ptr");
    return nullptr;
  }
}

std::string GraphManager::add_graph_node(const std::shared_ptr<GraphNode> &p_graph_node,
                                         const std::string                &graph_id)
{
  Logger::log()->trace("GraphManager::add_graph_node: {}", graph_id);

  std::string new_graph_id = gnode::Graph::add_node(p_graph_node, graph_id);
  p_graph_node->set_id(new_graph_id);
  p_graph_node->set_p_worker(this->worker.get());

  p_graph_node->update_progress = [this](const std::string &node_id, float progress)
  { this->on_update_progress(node_id, progress); };

  p_graph_node->global_output_updated = [this](const std::string &gid)
  { this->on_graph_node_output_updated(gid); };

  return new_graph_id;
}

void GraphManager::clear()
{
  Logger::log()->trace("GraphManager::clear");

  this->worker->cancel();
  this->pending_global_queue.clear();
  this->active_global_graph_id.clear();

  for (auto &[_, p_node] : this->nodes)
    if (auto *graph = dynamic_cast<GraphNode *>(p_node.get()))
      graph->gnode::Graph::clear();

  gnode::Graph::clear();
}

GraphNodeMap GraphManager::get_graph_nodes() const
{
  GraphNodeMap result;
  for (const auto &[gid, p_node] : this->nodes)
  {
    if (auto gno = std::dynamic_pointer_cast<GraphNode>(p_node))
      result[gid] = std::move(gno);
  }
  return result;
}

std::vector<std::string> GraphManager::get_graph_order() const
{
  std::vector<std::string> all_ids;
  all_ids.reserve(this->nodes.size());
  for (const auto &[gid, _] : this->nodes)
    all_ids.push_back(gid);

  return this->topological_sort(all_ids);
}

int GraphManager::get_graph_order_index(const std::string &graph_id) const
{
  if (this->is_graph_id_available(graph_id))
  {
    Logger::log()->critical("GraphManager::get_graph_order_index, graph ID not known: {}",
                            graph_id);
    throw std::runtime_error("Graph ID not known: " + graph_id);
  }

  auto order = this->get_graph_order();
  auto itr = std::find(order.begin(), order.end(), graph_id);
  return static_cast<int>(std::distance(order.begin(), itr));
}

GraphNode *GraphManager::get_graph_ref_by_id(const std::string &graph_id) const
{
  return dynamic_cast<GraphNode *>(this->get_node(graph_id));
}

gnode::GraphWorker &GraphManager::get_worker() { return *this->worker; }

bool GraphManager::is_graph_id_available(const std::string &graph_id) const
{
  return !this->nodes.contains(graph_id);
}

void GraphManager::remove_graph_node(const std::string &graph_id)
{
  Logger::log()->trace("GraphManager::remove_graph_node: graph_id {}", graph_id);

  if (this->is_graph_id_available(graph_id))
    return;

  if (auto *graph = this->get_graph_ref_by_id(graph_id))
    this->worker->cancel(static_cast<gnode::Graph *>(graph));

  std::erase(this->pending_global_queue, graph_id);
  if (this->active_global_graph_id == graph_id)
    this->active_global_graph_id.clear();

  gnode::Graph::remove_node(graph_id);
}

void GraphManager::reseed(bool backward)
{
  Logger::log()->trace("GraphManager::reseed");

  for (auto &[_, p_node] : this->nodes)
    if (auto *graph = dynamic_cast<GraphNode *>(p_node.get()))
      graph->reseed(backward);
}

std::string GraphManager::runtime_info_to_string(const char separator) const
{
  std::string result;

  for (const auto &graph_id : this->get_graph_order())
  {
    auto *graph = this->get_graph_ref_by_id(graph_id);
    if (!graph)
      continue;

    std::string graph_info = graph->runtime_info_to_string(separator);
    if (!graph_info.empty())
    {
      if (!result.empty())
        result += '\n';
      result += graph_info;
    }
  }

  return result;
}

void GraphManager::advance_global_queue()
{
  while (!this->pending_global_queue.empty())
  {
    std::string next_id = this->pending_global_queue.front();
    this->pending_global_queue.pop_front();

    if (auto *graph = this->get_graph_ref_by_id(next_id))
    {
      this->active_global_graph_id = next_id;
      graph->pull_inputs_from_parent();
      graph->update();
      return;
    }
  }

  this->active_global_graph_id.clear();
}

void GraphManager::on_graph_node_output_updated(const std::string &graph_id)
{
  if (!this->active_global_graph_id.empty())
  {
    if (this->active_global_graph_id == graph_id)
      this->advance_global_queue();
    return;
  }

  // A single GraphNode was edited directly in its tab: propagate to downstream
  // GraphNodes in topological order.
  auto downstream = this->get_connectivity_downstream(graph_id);
  if (downstream.empty())
    return;

  std::vector<std::string> to_update = this->get_nodes_to_update(downstream);
  if (to_update.empty())
    return;

  for (const auto &gid : to_update)
    this->pending_global_queue.push_back(gid);

  this->advance_global_queue();
}

void GraphManager::update()
{
  HSD_PROFILE_ZONE("GraphManager::update");
  Logger::log()->trace("GraphManager::update()");

  std::vector<std::string> order = this->get_graph_order();
  if (order.empty())
    return;

  if (!gnode::GraphWorker::enabled())
  {
    try
    {
      for (const auto &gid : order)
        if (auto *graph = this->get_graph_ref_by_id(gid))
        {
          graph->pull_inputs_from_parent();
          graph->gnode::Graph::update();
          graph->push_outputs_to_parent();
        }
    }
    catch (...)
    {
      this->on_update_failed(std::current_exception());
    }
    return;
  }

  this->pending_global_queue.clear();
  for (const auto &gid : order)
    this->pending_global_queue.push_back(gid);

  this->advance_global_queue();
}

void GraphManager::update(const std::string &graph_id)
{
  this->update(std::vector<std::string>{graph_id});
}

void GraphManager::update(const std::vector<std::string> &graph_ids)
{
  HSD_PROFILE_ZONE("GraphManager::update(graph_ids)");

  std::vector<std::string> order = this->get_nodes_to_update(graph_ids);
  if (order.empty())
    return;

  if (!gnode::GraphWorker::enabled())
  {
    try
    {
      for (const auto &gid : order)
        if (auto *graph = this->get_graph_ref_by_id(gid))
        {
          graph->pull_inputs_from_parent();
          graph->gnode::Graph::update();
          graph->push_outputs_to_parent();
        }
    }
    catch (...)
    {
      this->on_update_failed(std::current_exception());
    }
    return;
  }

  this->pending_global_queue.clear();
  for (const auto &gid : order)
    this->pending_global_queue.push_back(gid);

  this->advance_global_queue();
}

void GraphManager::on_update_progress(const std::string & /* node_id */, float progress)
{
  if (this->update_progress)
    this->update_progress(progress);
}

void GraphManager::on_update_failed(std::exception_ptr error)
{
  std::string failure;

  try
  {
    std::rethrow_exception(error);
  }
  catch (const std::bad_alloc &)
  {
    failure = "Out of memory: the graph update was abandoned. Lower the "
              "resolution and try again.";
  }
  catch (const std::exception &e)
  {
    failure = std::string("The graph update was abandoned: ") + e.what();
  }
  catch (...)
  {
    failure = "The graph update was abandoned after an unknown error.";
  }

  Logger::log()->critical("GraphManager::update: {}", failure);

  this->pending_global_queue.clear();
  this->active_global_graph_id.clear();

  if (this->update_progress)
    this->update_progress(100.f);

  for (auto &[_, p_node] : this->nodes)
    if (auto *graph = dynamic_cast<GraphNode *>(p_node.get()))
      if (graph->graph_update_finished)
        graph->graph_update_finished();

  if (this->update_failed)
    this->update_failed(failure);
}

void GraphManager::json_from(nlohmann::json const &json)
{
  this->json_from(json, nullptr);
}

void GraphManager::json_from(nlohmann::json const &json, GraphConfig *p_config)
{
  Logger::log()->trace("GraphManager::json_from");

  this->clear();

  uint id_count_val = 0;
  json_safe_get(json, "id_count", id_count_val);
  this->set_id_count(id_count_val);

  std::vector<std::string> graph_ids;
  if (json.contains("graph_order") && json["graph_order"].is_array())
  {
    graph_ids = json["graph_order"].get<std::vector<std::string>>();
  }
  else if (json.contains("graph_nodes") && json["graph_nodes"].is_object())
  {
    for (const auto &[gid, _] : json["graph_nodes"].items())
      graph_ids.push_back(gid);
  }

  for (const auto &graph_id : graph_ids)
  {
    auto config = std::make_shared<hesiod::GraphConfig>();
    auto graph = std::make_shared<hesiod::GraphNode>(graph_id, config);

    try
    {
      this->add_graph_node(graph, graph_id);

      if (json.contains("graph_nodes") && json["graph_nodes"].contains(graph_id))
      {
        graph->json_from(json["graph_nodes"][graph_id], p_config);
      }
      else
      {
        HSD_CTX.get_error_manager().push_error(
            ErrorCategory::Deserialization,
            std::format("Graph '{}': Missing key \"graph_nodes\" or \"{}\"",
                        graph_id,
                        graph_id));
      }
    }
    catch (const std::exception &e)
    {
      HSD_CTX.get_error_manager().push_error(
          ErrorCategory::Deserialization,
          std::format("Failed to add graph '{}': {}", graph_id, e.what()));
    }
    catch (...)
    {
      HSD_CTX.get_error_manager().push_error(
          ErrorCategory::Deserialization,
          std::format("Failed to add graph '{}': unknown error", graph_id));
    }
  }

  if (json.contains("links") && json["links"].is_array())
  {
    for (const auto &json_link : json["links"])
    {
      std::string node_id_from = "", port_id_from = "", node_id_to = "", port_id_to = "";
      json_safe_get(json_link, "node_id_from", node_id_from);
      json_safe_get(json_link, "port_id_from", port_id_from);
      json_safe_get(json_link, "node_id_to", node_id_to);
      json_safe_get(json_link, "port_id_to", port_id_to);

      try
      {
        this->new_link(node_id_from, port_id_from, node_id_to, port_id_to);
      }
      catch (const std::exception &e)
      {
        HSD_CTX.get_error_manager().push_error(
            ErrorCategory::LinkCreation,
            std::format("GraphManager: Failed to create link {}/{} => {}/{}: {}",
                        node_id_from,
                        port_id_from,
                        node_id_to,
                        port_id_to,
                        e.what()));
      }
    }
  }
}

nlohmann::json GraphManager::json_to() const
{
  Logger::log()->trace("GraphManager::json_to");

  nlohmann::json json;
  json["id_count"] = this->get_id_count();
  json["graph_order"] = this->get_graph_order();

  nlohmann::json json_graphs = nlohmann::json::object();
  for (const auto &[graph_id, p_node] : this->nodes)
    if (auto *graph = dynamic_cast<GraphNode *>(p_node.get()))
      json_graphs[graph_id] = graph->json_to();

  json["graph_nodes"] = json_graphs;

  nlohmann::json json_links = nlohmann::json::array();
  for (const auto &link : this->links)
  {
    gnode::Node *p_from = this->get_node(link.from);
    gnode::Node *p_to = this->get_node(link.to);
    if (!p_from || !p_to)
      continue;

    nlohmann::json json_link;
    json_link["node_id_from"] = link.from;
    json_link["node_id_to"] = link.to;
    json_link["port_id_from"] = p_from->get_port_label(link.port_from);
    json_link["port_id_to"] = p_to->get_port_label(link.port_to);
    json_links.push_back(json_link);
  }
  json["links"] = json_links;

  return json;
}

void GraphManager::load_from_file(const std::string &fname, GraphConfig *p_config)
{
  Logger::log()->trace("GraphManager::load_from_file: fname {}", fname);

  nlohmann::json json = json_from_file(fname);
  this->json_from(json["graph_manager"], p_config);
  this->update();
}

void GraphManager::save_to_file(const std::string &fname) const
{
  Logger::log()->trace("GraphManager::save_to_file: fname {}", fname);

  nlohmann::json json;
  json["graph_manager"] = this->json_to();
  json_to_file(json, fname);
}

} // namespace hesiod
