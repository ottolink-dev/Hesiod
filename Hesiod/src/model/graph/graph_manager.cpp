/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "nlohmann/json.hpp"

#include "highmap/coord_frame.hpp"
#include "highmap/interpolate/interpolate_array.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/graph/graph_config.hpp"
#include "hesiod/model/graph/graph_manager.hpp"
#include "hesiod/model/graph/graph_node.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

GraphManager::GraphManager(const std::string &id) : id(id)
{
  Logger::log()->trace("GraphManager::GraphManager: id: {}", id);
}

std::string GraphManager::add_graph_node(const std::shared_ptr<GraphNode> &p_graph_node,
                                         const std::string                &graph_id)
{
  Logger::log()->trace("GraphManager::add_graph_node: {}", graph_id);

  std::string new_graph_id = graph_id;

  // use node pointer as ID if none is provided
  if (graph_id.empty())
    new_graph_id = std::to_string(this->id_count++);

  // check if the ID is available
  if (!this->is_graph_id_available(new_graph_id))
    throw std::runtime_error("Graph ID already used: " + new_graph_id);

  // add the graph to the map and store the ID within the graph (in case of)
  this->graph_nodes[new_graph_id] = p_graph_node;
  this->graph_order.push_back(new_graph_id);

  // setup new graph node
  {
    p_graph_node->set_id(new_graph_id);

    // store a reference to the global storage of broadcasting data
    p_graph_node->set_p_broadcast_params(&broadcast_params);

    // connections for broadcasting data between graph_nodes
    p_graph_node->broadcast_node_updated =
        [this](const std::string &graph_id, const std::string &tag)
    { this->on_broadcast_node_updated(graph_id, tag); };

    p_graph_node->new_broadcast_tag = [this](const std::string        &tag,
                                             const hmap::CoordFrame   *t_source,
                                             const hmap::VirtualArray *h_source)
    { this->on_new_broadcast_tag(tag, t_source, h_source); };

    p_graph_node->remove_broadcast_tag = [this](const std::string &tag)
    { this->on_remove_broadcast_tag(tag); };

    // gather update progress signals
    p_graph_node->update_progress = [this](const std::string &node_id, float progress)
    { this->on_update_progress(node_id, progress); };
  }

  return new_graph_id;
}

void GraphManager::clear()
{
  Logger::log()->trace("GraphManager::clear");

  for (auto &[_, graph] : this->graph_nodes)
    graph->clear();

  this->id_count = 0;
  this->graph_nodes.clear();
  this->graph_order.clear();
  this->broadcast_params.clear();
}

void GraphManager::export_flatten()
{
  Logger::log()->trace("GraphManager::export_flatten");

  // create config
  auto export_cfg = GraphConfig();
  export_cfg.set_shape(export_param.shape);
  export_cfg.set_tiling(export_param.tiling);
  export_cfg.set_overlap(export_param.overlap);

  // target heightmap
  hmap::VirtualArray h_export(export_cfg.shape,
                              export_cfg.tile_shape,
                              export_cfg.halo,
                              export_cfg.storage_mode);

  // compute global bounding box
  glm::vec4 bbox_global(std::numeric_limits<float>::max(),
                        std::numeric_limits<float>::lowest(),
                        std::numeric_limits<float>::max(),
                        std::numeric_limits<float>::lowest());

  for (auto &graph_id : this->get_graph_order())
  {
    glm::vec4 bbox = this->get_graph_nodes().at(graph_id)->compute_bounding_box();

    bbox_global = glm::vec4(std::min(bbox_global.x, bbox.x),
                            std::max(bbox_global.y, bbox.y),
                            std::min(bbox_global.z, bbox.z),
                            std::max(bbox_global.w, bbox.w));
  }

  // retrieve for each graph the selected tag and the corresponding data
  std::vector<const hmap::VirtualArray *> h_sources;
  std::vector<const hmap::CoordFrame *>   t_sources;

  for (auto ids : this->export_param.ids)
  {
    std::string graph_id = std::get<0>(ids);
    std::string node_id = std::get<1>(ids);
    std::string port_id = std::get<2>(ids);

    hmap::VirtualArray *p_h = this->graph_nodes.at(graph_id)
                                  ->get_node_ref_by_id(node_id)
                                  ->get_value_ref<hmap::VirtualArray>(port_id);

    hmap::CoordFrame *p_t = dynamic_cast<hmap::CoordFrame *>(
        this->graph_nodes.at(graph_id).get());

    h_sources.push_back(p_h);
    t_sources.push_back(p_t);
  }

  // bbox_global is (min_x, max_x, min_y, max_y); origin is the lower corner
  glm::vec2        origin(bbox_global.x, bbox_global.z);
  glm::vec2        size(bbox_global.y - bbox_global.x, bbox_global.w - bbox_global.z);
  float            rotation_angle = 0.f;
  hmap::CoordFrame frame_export(origin, size, rotation_angle);

  hmap::flatten_heightmap(h_sources,
                          h_export,
                          t_sources,
                          frame_export,
                          export_cfg.cm_cpu);

  // guard the output path: OpenCV's imwrite throws (and, uncaught, aborts the
  // whole application) if the path is empty or has no extension it recognises
  std::filesystem::path out_path = export_param.export_path;

  if (out_path.empty())
  {
    Logger::log()->error("GraphManager::export_flatten: export path is empty, "
                         "nothing was written");
    return;
  }

  if (out_path.extension().empty())
  {
    out_path.replace_extension(".png");
    Logger::log()->warn("GraphManager::export_flatten: export path had no "
                        "extension, defaulting to {}",
                        out_path.string());
  }

  // catch any writer error (unsupported extension, unwritable path, ...) so a
  // failed export reports an error instead of crashing the app
  try
  {
    // raw heightmap
    h_export.to_array(export_cfg.cm_cpu).to_png_grayscale(out_path.string(), CV_16U);

    // with hillshading
    const std::filesystem::path fname_hs = insert_before_extension(out_path,
                                                                   "_preview");
    h_export.to_array(export_cfg.cm_cpu)
        .to_png(fname_hs.string(), hmap::Cmap::TERRAIN, true);
  }
  catch (const std::exception &e)
  {
    Logger::log()->error("GraphManager::export_flatten: failed to write export "
                         "image '{}': {}",
                         out_path.string(),
                         e.what());
  }
}

const BroadcastMap &GraphManager::get_broadcast_params()
{
  return this->broadcast_params;
}

FlattenConfig GraphManager::get_export_param() const { return this->export_param; }

const GraphNodeMap &GraphManager::get_graph_nodes() { return this->graph_nodes; }

const std::vector<std::string> &GraphManager::get_graph_order()
{
  return this->graph_order;
}

int GraphManager::get_graph_order_index(const std::string &graph_id)
{
  if (this->is_graph_id_available(graph_id))
  {
    Logger::log()->critical("GraphManager::get_graph_order_index, graph ID not known: {}",
                            graph_id);
    throw std::runtime_error("Graph ID not known: " + graph_id);
  }

  auto itr = std::find(this->graph_order.begin(), this->graph_order.end(), graph_id);
  return (int)std::distance(this->graph_order.begin(), itr);
}

GraphNode *GraphManager::get_graph_ref_by_id(const std::string &graph_id)
{
  auto it = graph_nodes.find(graph_id);
  if (it == graph_nodes.end())
    return nullptr;
  else
    return it->second.get();
}

std::string GraphManager::get_id() const { return this->id; }

std::shared_ptr<GraphManager> GraphManager::get_shared()
{
  try
  {
    return shared_from_this();
  }
  catch (...)
  {
    Logger::log()->critical(
        "GraphManager::get_shared: object is not managed by shared_ptr");
    return nullptr;
  }
}

bool GraphManager::is_graph_above(const std::string &graph_id,
                                  const std::string &ref_graph_id)
{
  for (auto &gid : this->graph_order)
  {
    if (gid == graph_id)
      return HSD_CTX.app_settings.model.allow_broadcast_receive_within_same_graph;
    else if (gid == ref_graph_id)
      return true;
  }

  return false;
}

bool GraphManager::is_graph_id_available(const std::string &graph_id)
{
  return !this->graph_nodes.contains(graph_id);
}

void GraphManager::json_from(nlohmann::json const &json)
{
  this->json_from(json, nullptr);
}

void GraphManager::json_from(nlohmann::json const &json, GraphConfig *p_config)
{
  Logger::log()->trace("GraphManager::json_from");

  // clean-up current state
  this->clear();

  // keep deserializing
  json_safe_get(json, "id_count", this->id_count);
  this->export_param.json_from(json["export_param"]);

  // graph order
  if (json.contains("graph_order"))
  {
    std::vector<std::string> gid_order = json["graph_order"];

    for (auto &graph_id : gid_order)
    {
      Logger::log()->trace("graph graph_id: {}", graph_id);

      // dummy default config that will be overriden after by the
      // GraphNode instances during their 'json_from' deserialization
      auto config = std::make_shared<hesiod::GraphConfig>();
      auto graph = std::make_shared<hesiod::GraphNode>("", config);

      // in this order (add, then deserialize), add the graph node to
      // the graph manager and then deserialize the graph node because
      // the Receive nodes, if any, need a reference to the
      // broadcast_params of the GraphManager, which is provided to the
      // graph node when added...
      this->add_graph_node(graph, graph_id);

      if (json.contains("graph_nodes") && json["graph_nodes"].contains(graph_id))
      {
        graph->json_from(json["graph_nodes"][graph_id], p_config);
      }
      else
      {
        Logger::log()->error("Missing key \"graph_nodes\" or \"{}\"", graph_id);
      }
    }
  }
  else
  {
    Logger::log()->error("Missing key \"graph_order\" in json");
  }
}

nlohmann::json GraphManager::json_to() const
{
  Logger::log()->trace("GraphManager::json_to");

  nlohmann::json json;

  // NB - broadcast_params is not serialized since it is regenerated
  // when re-creating the graph nodes
  json["id_count"] = this->id_count;
  json["graph_order"] = this->graph_order;
  json["export_param"] = this->export_param.json_to();

  // graphs
  nlohmann::json json_graphs;
  for (auto &[graph_id, graph] : this->graph_nodes)
    json_graphs[graph_id] = graph->json_to();

  json["graph_nodes"] = json_graphs;

  return json;
}

void GraphManager::load_from_file(const std::string &fname, GraphConfig *p_config)
{
  Logger::log()->trace("GraphManager::load_from_file: fname {}", fname);

  nlohmann::json json = json_from_file(fname);
  this->json_from(json["graph_manager"], p_config);

  // update graphs
  this->update();
}

void GraphManager::on_broadcast_node_updated(const std::string &graph_id,
                                             const std::string &tag)
{
  Logger::log()->trace("GraphManager::on_broadcast_node_updated: broadcasting {}", tag);

  for (auto &[gid, graph] : this->graph_nodes)
  {
    // prevent any broadcast from a top layer to a sublayer, this
    // could lead to endless loop in case of cross-broadcast
    if (this->is_graph_above(gid, graph_id))
      graph->on_broadcast_node_updated(tag);
  }
}

void GraphManager::on_new_broadcast_tag(const std::string        &tag,
                                        const hmap::CoordFrame   *t_source,
                                        const hmap::VirtualArray *h_source)
{
  Logger::log()->trace("GraphManager::on_new_broadcast_tag: tag {}", tag);

  this->broadcast_params[tag] = BroadcastParam(t_source, h_source);

  if (this->new_broadcast_tag)
    this->new_broadcast_tag(tag);
}

void GraphManager::on_remove_broadcast_tag(const std::string &tag)
{
  Logger::log()->trace("GraphManager::on_remove_broadcast_tag: tag {}", tag);

  this->broadcast_params.erase(tag);

  if (this->remove_broadcast_tag)
    this->remove_broadcast_tag(tag);
}

void GraphManager::on_update_progress(const std::string & /* node_id */, float progress)
{
  if (this->update_progress)
    this->update_progress(progress);
}

void GraphManager::remove_graph_node(const std::string &graph_id)
{
  Logger::log()->trace("GraphManager::remove_graph_node: graph_id {}", graph_id);

  if (this->is_graph_id_available(graph_id))
    return;

  this->graph_order.erase(
      std::remove(this->graph_order.begin(), this->graph_order.end(), graph_id),
      this->graph_order.end());

  this->graph_nodes.erase(graph_id);
}

void GraphManager::reseed(bool backward)
{
  Logger::log()->trace("GraphManager::reseed");

  for (auto &[_, graph] : this->graph_nodes)
    if (graph)
      graph->reseed(backward);
}

void GraphManager::save_to_file(const std::string &fname) const
{
  Logger::log()->trace("GraphManager::save_to_file: fname {}", fname);

  // fill-in json with graph data
  nlohmann::json json;
  json["graph_manager"] = this->json_to();
  json_to_file(json, fname);
}

void GraphManager::set_export_param(const FlattenConfig &new_export_param)
{
  this->export_param = new_export_param;
}

void GraphManager::set_graph_order(const std::vector<std::string> &new_graph_order)
{
  this->graph_order = new_graph_order;
}

void GraphManager::set_id(const std::string &new_id) { this->id = new_id; }

void GraphManager::update()
{
  Logger::log()->trace("GraphManager::update()");

  for (auto &graph_id : this->graph_order)
    this->graph_nodes.at(graph_id)->update();
}

} // namespace hesiod
