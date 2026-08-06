/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/broadcast_node.hpp"

namespace hesiod
{

BroadcastNode::BroadcastNode(const std::string &label, std::weak_ptr<GraphConfig> config)
    : BaseNode(label, config)
{
}

void BroadcastNode::generate_broadcast_tag()
{
  this->broadcast_tag = this->get_graph_id() + "/" + this->get_label() + "/" +
                        this->get_id();

  // set the attribute value accordingly so that the tag is visible in the GUI for the
  // user
  this->get_attr_ref<attr::StringAttribute>("tag")->set_value(this->broadcast_tag);
}

std::string BroadcastNode::get_broadcast_tag()
{
  this->generate_broadcast_tag();
  return this->broadcast_tag;
}

void BroadcastNode::json_from(nlohmann::json const &json)
{
  BaseNode::json_from(json);
  this->broadcast_tag = json["broadcast_tag"];
}

nlohmann::json BroadcastNode::json_to() const
{
  nlohmann::json json;

  json = BaseNode::json_to();
  json["broadcast_tag"] = this->broadcast_tag;

  return json;
}

} // namespace hesiod
