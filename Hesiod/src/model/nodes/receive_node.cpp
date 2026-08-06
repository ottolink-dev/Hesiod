/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/receive_node.hpp"

namespace hesiod
{

ReceiveNode::ReceiveNode(const std::string &label, std::weak_ptr<GraphConfig> config)
    : BaseNode(label, config)
{
}

std::string ReceiveNode::get_current_tag() const
{
  return this->get_attr<attr::ChoiceAttribute>("tag");
}

BroadcastMap *ReceiveNode::get_p_broadcast_params()
{
  if (!this->p_broadcast_params)
  {
    Logger::log()->warn(
        "ReceiveNode::get_p_broadcast_params: p_broadcast_param is nullptr");
  }

  return this->p_broadcast_params;
}

hmap::CoordFrame *ReceiveNode::get_p_coord_frame() { return this->p_coord_frame; }

void ReceiveNode::set_p_broadcast_params(BroadcastMap *new_p_broadcast_params)
{
  this->p_broadcast_params = new_p_broadcast_params;
}

void ReceiveNode::set_p_coord_frame(hmap::CoordFrame *new_p_coord_frame)
{
  this->p_coord_frame = new_p_coord_frame;
}

void ReceiveNode::update_tag_list(const std::vector<std::string> &new_tags)
{
  auto handle = this->get_attr_ref<attr::ChoiceAttribute>("tag");

  // add option to select nothing
  std::vector<std::string> new_tags_mod = new_tags;
  new_tags_mod.emplace(new_tags_mod.begin(), "NO TAG");

  // --- update attribute

  handle->set_choice_list(new_tags_mod);

  // if the currently selected tag is not available anymore, set a dummy value
  std::string current_tag = handle->get_value();

  Logger::log()->trace("ReceiveNode::update_tag_list: current tag value: {}",
                       current_tag);

  if (std::find(new_tags_mod.begin(), new_tags_mod.end(), current_tag) ==
      new_tags_mod.end())
    handle->set_value(new_tags_mod.front());
}

} // namespace hesiod
