/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include "hesiod/model/nodes/subgraph_node.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/graph/graph_node.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

SubGraphNode::SubGraphNode(const std::string                &label,
                           std::weak_ptr<GraphConfig>        parent_config,
                           const std::shared_ptr<GraphNode> &subgraph)
    : BaseNode(label, parent_config), hmap::CoordFrame(), subgraph(subgraph)
{
  Logger::log()->trace("SubGraphNode::SubGraphNode: label: {}", label);

  this->subgraph_impl = std::make_shared<gnode::SubGraphNode>(label, this->subgraph);
}

std::shared_ptr<GraphNode> SubGraphNode::get_subgraph() const
{
  return this->subgraph;
}

void SubGraphNode::set_subgraph(const std::shared_ptr<GraphNode> &new_subgraph)
{
  this->subgraph = new_subgraph;

  if (this->subgraph_impl)
    this->subgraph_impl->set_subgraph(this->subgraph);
}

void SubGraphNode::auto_bind_ports()
{
  if (this->subgraph_impl)
    this->subgraph_impl->auto_bind_ports();
}

void SubGraphNode::compute()
{
  Logger::log()->trace("SubGraphNode::compute: label: {}", this->get_label());

  if (this->subgraph_impl)
    this->subgraph_impl->compute();
}

void SubGraphNode::json_from(nlohmann::json const &json)
{
  BaseNode::json_from(json);

  // CoordFrame
  std::vector<float> vo = {};
  std::vector<float> vs = {};

  json_safe_get(json, "origin", vo);
  json_safe_get(json, "size", vs);

  if (vo.size() >= 2)
    this->set_origin(glm::vec2(vo[0], vo[1]));
  if (vs.size() >= 2)
    this->set_size(glm::vec2(vs[0], vs[1]));

  float rotation_angle = 0.f;
  json_safe_get(json, "rotation_angle", rotation_angle);
  this->set_rotation_angle(rotation_angle);

  // Subgraph
  if (json.contains("subgraph"))
  {
    if (!this->subgraph)
    {
      auto cfg = std::make_shared<GraphConfig>();
      this->subgraph = std::make_shared<GraphNode>(this->get_id() + "_inner", cfg);
      this->set_subgraph(this->subgraph);
    }
    this->subgraph->json_from(json["subgraph"]);
  }
}

nlohmann::json SubGraphNode::json_to() const
{
  nlohmann::json json = BaseNode::json_to();

  // CoordFrame
  json["origin"] = {this->get_origin().x, this->get_origin().y};
  json["size"] = {this->get_size().x, this->get_size().y};
  json["rotation_angle"] = this->get_rotation_angle();

  // Subgraph
  if (this->subgraph)
    json["subgraph"] = this->subgraph->json_to();

  return json;
}

} // namespace hesiod
