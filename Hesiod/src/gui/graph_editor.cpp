/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/gui/graph_editor.hpp"

#include <stdexcept>
#include <utility>

#include "hesiod/model/graph/graph_node.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/legacy/legacy_converter.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{
GraphEditor::GraphEditor(std::weak_ptr<GraphNode> graph,
                         gngui::GraphViewer      &view,
                         NodePresentation         presentation)
    : model(std::move(graph)), view(view), presentation(std::move(presentation))
{
}

GraphEditor::Batch::Batch(GraphEditor &editor)
    : editor(editor), previous_dirty(editor.dirty),
      previous_full_update(editor.full_update), previous_changed(editor.changed),
      previous_notifications(editor.notifications.size())
{
  ++editor.batch_depth;
}

GraphEditor::Batch::~Batch()
{
  if (active)
  {
    editor.dirty = std::move(previous_dirty);
    editor.full_update = previous_full_update;
    editor.changed = previous_changed;
    editor.notifications.resize(previous_notifications);
    --editor.batch_depth;
  }
}

void GraphEditor::Batch::commit()
{
  if (!active)
    return;
  active = false;
  if (--editor.batch_depth == 0)
    editor.finish_batch();
}

std::shared_ptr<GraphNode> GraphEditor::graph() const
{
  auto graph = model.lock();
  if (!graph)
    throw std::runtime_error("The graph is no longer available.");
  return graph;
}

void GraphEditor::finish_batch()
{
  auto       graph = this->graph();
  auto       pending = std::exchange(notifications, {});
  auto       pending_dirty = std::exchange(dirty, {});
  const bool update_all = std::exchange(full_update, false);
  const bool edited = std::exchange(changed, false);

  // Publish only once model and scene agree. Clear scheduling state first so a
  // callback or a failed computation cannot leave future edits blocked.
  for (const auto &notice : pending)
  {
    const auto &callback = notice.created ? presentation.created : presentation.deleted;
    if (callback)
      callback(notice.id);
  }

  if (edited && presentation.changed)
    presentation.changed();

  if (update_all)
    graph->update();
  else
  {
    std::vector<std::string> ids;
    for (const auto &id : pending_dirty)
      if (graph->get_node(id))
        ids.push_back(id);
    if (!ids.empty())
      graph->update(ids);
  }
}

void GraphEditor::request_update(const std::vector<std::string> &ids)
{
  Batch batch(*this);
  if (ids.empty())
    full_update = true;
  else
    dirty.insert(ids.begin(), ids.end());
  batch.commit();
}

std::vector<GraphEditor::Link> GraphEditor::links_for(const std::string &id) const
{
  std::vector<Link> result;
  for (const auto &link : graph()->get_link_views(id))
    result.push_back({link.from, link.port_label_from, link.to, link.port_label_to});
  return result;
}

bool GraphEditor::compatible(const Link &link) const
{
  auto  graph = this->graph();
  auto *from = graph->get_node(link.node_out);
  auto *to = graph->get_node(link.node_in);
  if (!from || !to || from == to)
    return false;
  const int output = from->get_port_index(link.port_out);
  const int input = to->get_port_index(link.port_in);
  return output >= 0 && input >= 0 &&
         from->get_port_type(link.port_out) == gnode::PortType::OUT &&
         to->get_port_type(link.port_in) == gnode::PortType::IN &&
         from->get_data_type(output) == to->get_data_type(input);
}

void GraphEditor::validate_connection(const Link &link) const
{
  if (!compatible(link))
    throw std::invalid_argument("Cannot connect the selected nodes: incompatible ports.");
  if (graph()->is_reachable(link.node_in, link.node_out))
    throw std::invalid_argument(
        "Cannot connect the selected nodes: this would create a cycle.");
  auto *from = view.get_graphics_node_by_id(link.node_out);
  auto *to = view.get_graphics_node_by_id(link.node_in);
  if (!from || !to || from->get_port_index(link.port_out) < 0 ||
      to->get_port_index(link.port_in) < 0)
    throw std::runtime_error("Cannot connect nodes missing from the scene.");
}

std::string GraphEditor::add_node(const std::string                     &type,
                                  QPointF                                position,
                                  const std::function<void(BaseNode &)> &initialize)
{
  if (type.empty())
    return {};
  auto              graph = this->graph();
  Batch             batch(*this);
  const std::string id = graph->add_node(type);
  try
  {
    auto *node = graph->get_node_ref_by_id<BaseNode>(id);
    if (initialize)
      initialize(*node);
    node->set_id(id); // imported settings must not restore the original ID
    presentation.create(id, position);
    if (!view.get_graphics_node_by_id(id))
      throw std::runtime_error("Could not create the graphics node.");
  }
  catch (...)
  {
    // Proxies still refer to live model nodes while their graphics are erased.
    graph->get_node_ref_by_id<BaseNode>(id)->set_id(id);
    view.erase_node(id);
    graph->remove_node(id);
    throw;
  }
  notifications.push_back({id, true});
  changed = true;
  dirty.insert(id);
  batch.commit();
  return id;
}

bool GraphEditor::connect(const Link &link)
{
  auto graph = this->graph();
  validate_connection(link);
  const auto adjacent = links_for(link.node_in);
  if (hesiod::contains(adjacent, link))
    return false;

  std::vector<Link> previous;
  for (const auto &old : adjacent)
    if (old.node_in == link.node_in && old.port_in == link.port_in)
      previous.push_back(old);

  Batch batch(*this);
  // Validate before disturbing the old input, and keep its graphics until the
  // model accepts the replacement. Restore both sides if synchronization fails.
  try
  {
    for (const auto &old : previous)
      graph->remove_link(old.node_out, old.port_out, old.node_in, old.port_in);
    if (!graph->new_link(link.node_out, link.port_out, link.node_in, link.port_in))
      throw std::runtime_error("The graph did not accept the connection.");
    for (const auto &old : previous)
      view.erase_link(old);
    view.add_link(link.node_out, link.port_out, link.node_in, link.port_in);
  }
  catch (...)
  {
    graph->remove_link(link.node_out, link.port_out, link.node_in, link.port_in);
    view.erase_link(link);
    for (const auto &old : previous)
    {
      graph->new_link(old.node_out, old.port_out, old.node_in, old.port_in);
      view.erase_link(old);
      view.add_link(old.node_out, old.port_out, old.node_in, old.port_in);
    }
    throw;
  }
  dirty.insert(link.node_in);
  changed = true;
  batch.commit();
  return true;
}

bool GraphEditor::disconnect(const Link &link)
{
  auto       graph = this->graph();
  Batch      batch(*this);
  const bool removed = graph->remove_link(link.node_out,
                                          link.port_out,
                                          link.node_in,
                                          link.port_in);
  view.erase_link(link);
  if (removed)
  {
    dirty.insert(link.node_in);
    changed = true;
  }
  batch.commit();
  return removed;
}

void GraphEditor::erase(const std::vector<std::string> &node_ids,
                        const std::vector<Link>        &links)
{
  auto  graph = this->graph();
  Batch batch(*this);
  for (const auto &link : links)
    disconnect(link);
  const std::set<std::string> unique_ids(node_ids.begin(), node_ids.end());
  for (const auto &id : unique_ids)
  {
    if (!graph->get_node(id))
      continue;
    for (const auto &link : links_for(id))
      if (link.node_out == id)
        dirty.insert(link.node_in);
    view.erase_node(id);
    // GraphNode owns Broadcast/Receive cleanup; never bypass its remove_node.
    graph->remove_node(id);
    notifications.push_back({id, false});
    changed = true;
    dirty.erase(id);
  }
  batch.commit();
}

void GraphEditor::clear()
{
  std::vector<std::string> ids;
  for (const auto &[id, node] : graph()->get_nodes())
    ids.push_back(id);
  erase(ids);
  view.clear(); // comments and groups also belong to the cleared scene
}

void GraphEditor::restore_links(const std::vector<Link> &links)
{
  for (const auto &link : links)
    connect(link);
}

std::string GraphEditor::replace_node(const std::string &id, const std::string &type)
{
  auto  graph = this->graph();
  auto *graphics = view.get_graphics_node_by_id(id);
  if (!graph->get_node(id) || !graphics)
    throw std::invalid_argument("Select an existing node to replace.");
  const auto        previous = links_for(id);
  Batch             batch(*this);
  const std::string replacement = add_node(type, graphics->pos());
  if (replacement.empty())
    return {};
  try
  {
    for (auto link : previous)
    {
      if (link.node_out == id)
        link.node_out = replacement;
      if (link.node_in == id)
        link.node_in = replacement;
      if (compatible(link))
        connect(link);
    }
  }
  catch (...)
  {
    erase({replacement});
    restore_links(previous);
    throw;
  }
  // Keep the original node until replacement creation and reconnection succeed.
  erase({id});
  batch.commit();
  return replacement;
}

std::string GraphEditor::insert_node(const std::string &id,
                                     const std::string &type,
                                     QPointF            position)
{
  auto graph = this->graph();
  if (!graph->get_node(id) || !view.get_graphics_node_by_id(id))
    throw std::invalid_argument("Select an existing node to insert after.");
  const auto        previous = links_for(id);
  Batch             batch(*this);
  const std::string inserted = add_node(type, position);
  if (inserted.empty())
    return {};
  try
  {
    bool connected = false;
    for (const auto &old : previous)
    {
      if (old.node_out != id)
        continue;
      const Link upstream{id, old.port_out, inserted, old.port_in};
      const Link downstream{inserted, old.port_out, old.node_in, old.port_in};
      // Preserve branches unless both halves can reconnect.
      if (compatible(upstream) && compatible(downstream))
      {
        connect(upstream);
        connect(downstream);
        connected = true;
      }
    }
    if (!connected)
    {
      auto *from = graph->get_node(id);
      auto *to = graph->get_node(inserted);
      for (int out = 0; out < from->get_nports() && !connected; ++out)
        for (int in = 0; in < to->get_nports() && !connected; ++in)
        {
          Link link{id, from->get_port_label(out), inserted, to->get_port_label(in)};
          if (compatible(link))
          {
            connect(link);
            connected = true;
          }
        }
    }
  }
  catch (...)
  {
    erase({inserted});
    restore_links(previous);
    throw;
  }
  batch.commit();
  return inserted;
}

nlohmann::json GraphEditor::import_nodes(const nlohmann::json &json, QPointF origin)
{
  nlohmann::json result = convert_legacy_graph_widget_json(json);
  if (!result.contains("nodes") || result["nodes"].is_null())
    return result;
  if (!result["nodes"].is_array())
    throw std::invalid_argument("Imported nodes must be an array.");
  Batch                              batch(*this);
  std::map<std::string, std::string> ids;
  std::vector<std::string>           created;
  try
  {
    for (auto &node : result["nodes"])
    {
      const auto old_id = node.at("id").get<std::string>();
      if (ids.contains(old_id))
        throw std::invalid_argument("Duplicate node ID in the imported graph.");
      const QPointF offset(node.at("scene_position.x").get<double>(),
                           node.at("scene_position.y").get<double>());
      const auto    id = add_node(node.at("caption").get<std::string>(),
                               origin + offset,
                               [&](BaseNode &model)
                               {
                                 auto settings = node.at("settings");
                                 settings["id"] = model.get_id();
                                 model.json_from(settings);
                               });
      if (id.empty())
        throw std::invalid_argument("Missing node type in the imported graph.");
      created.push_back(id);
      ids.emplace(old_id, id);
      node["id"] = id;
      node["settings"]["id"] = id;
    }
    if (result.contains("links") && !result["links"].is_null())
    {
      if (!result["links"].is_array())
        throw std::invalid_argument("Imported links must be an array.");
      for (auto &link : result["links"])
      {
        const auto from = ids.at(link.at("node_out_id").get<std::string>());
        const auto to = ids.at(link.at("node_in_id").get<std::string>());
        const auto out = link.at("port_out_id").get<std::string>();
        const auto in = link.at("port_in_id").get<std::string>();
        connect({from, out, to, in});
        link["node_out_id"] = from;
        link["node_in_id"] = to;
      }
    }
  }
  catch (...)
  {
    erase(created);
    throw;
  }
  batch.commit();
  return result;
}
} // namespace hesiod
