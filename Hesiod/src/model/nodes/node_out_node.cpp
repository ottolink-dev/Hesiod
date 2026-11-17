/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/array.hpp"
#include "highmap/flora/forest.hpp"
#include "highmap/geometry/cloud.hpp"
#include "highmap/geometry/path.hpp"
#include "highmap/rocks/rock_field.hpp"
#include "highmap/virtual_array/virtual_array.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/node_out_node.hpp"

namespace hesiod
{

namespace
{

const std::vector<std::string> &boundary_data_types()
{
  static const std::vector<std::string> types = {
      "VirtualArray",
      "VirtualTexture",
      "Array",
      "Cloud",
      "Path",
      "Forest",
      "RockField",
  };
  return types;
}

void transfer_virtual_array(const hmap::VirtualArray *src,
                            const GraphConfig        &src_cfg,
                            hmap::VirtualArray       &dst,
                            const GraphConfig        &dst_cfg)
{
  if (!src || src->empty())
  {
    dst.fill(0.f, dst_cfg.cm_cpu);
    return;
  }

  if (src->shape == dst.shape && src->tile_shape == dst.tile_shape &&
      src->halo == dst.halo)
  {
    hmap::copy_data(*src, dst, dst_cfg.cm_cpu);
  }
  else
  {
    hmap::Array arr = src->to_array(src_cfg.cm_cpu);
    dst.from_array_bilinear(arr, dst_cfg.cm_cpu);
    dst.sync_overlap_buffers();
  }
}

void transfer_virtual_texture(const hmap::VirtualTexture *src,
                              const GraphConfig          &src_cfg,
                              hmap::VirtualTexture       &dst,
                              const GraphConfig          &dst_cfg)
{
  if (!src || src->channels() <= 0)
  {
    for (int c = 0; c < dst.channels(); ++c)
      dst.fill(c, c == 3 ? 1.f : 0.f, dst_cfg.cm_cpu);
    return;
  }

  const int nch = std::min(src->channels(), dst.channels());
  for (int c = 0; c < nch; ++c)
    transfer_virtual_array(&src->channel(c), src_cfg, dst.channel(c), dst_cfg);
}

} // namespace

NodeOutNode::NodeOutNode(const std::string &label, std::weak_ptr<GraphConfig> config)
    : BaseNode(label, config)
{
}

std::string NodeOutNode::get_slot_name() const
{
  return this->val<std::string>("slot_name");
}

std::string NodeOutNode::get_data_type_name() const
{
  return this->val<std::string>("data_type");
}

void NodeOutNode::rebuild_port()
{
  if (this->has_port("value"))
    this->remove_port("value");

  const std::string dtype = this->get_data_type_name();
  if (dtype == "VirtualArray")
    this->add_port<hmap::VirtualArray>(gnode::PortType::IN, "value");
  else if (dtype == "VirtualTexture")
    this->add_port<hmap::VirtualTexture>(gnode::PortType::IN, "value");
  else if (dtype == "Array")
    this->add_port<hmap::Array>(gnode::PortType::IN, "value");
  else if (dtype == "Cloud")
    this->add_port<hmap::Cloud>(gnode::PortType::IN, "value");
  else if (dtype == "Path")
    this->add_port<hmap::Path>(gnode::PortType::IN, "value");
  else if (dtype == "Forest")
    this->add_port<hmap::Forest>(gnode::PortType::IN, "value");
  else if (dtype == "RockField")
    this->add_port<hmap::RockField>(gnode::PortType::IN, "value");
  else
    this->add_port<hmap::VirtualArray>(gnode::PortType::IN, "value");
}

void NodeOutNode::register_on_parent(gnode::Node       *parent_node,
                                     const std::string &port_label) const
{
  auto      *parent_base = dynamic_cast<BaseNode *>(parent_node);
  const auto dtype = this->get_data_type_name();

  if (dtype == "VirtualArray")
  {
    if (parent_base)
      parent_node->add_port<hmap::VirtualArray>(gnode::PortType::OUT,
                                                port_label,
                                                CONFIG((*parent_base)));
    else
      parent_node->add_port<hmap::VirtualArray>(gnode::PortType::OUT,
                                                port_label,
                                                CONFIG((*this)));
  }
  else if (dtype == "VirtualTexture")
  {
    if (parent_base)
      parent_node->add_port<hmap::VirtualTexture>(gnode::PortType::OUT,
                                                  port_label,
                                                  CONFIG_TEX((*parent_base)));
    else
      parent_node->add_port<hmap::VirtualTexture>(gnode::PortType::OUT,
                                                  port_label,
                                                  CONFIG_TEX((*this)));
  }
  else if (dtype == "Array")
    parent_node->add_port<hmap::Array>(gnode::PortType::OUT,
                                       port_label,
                                       parent_base ? parent_base->cfg().shape
                                                   : this->cfg().shape);
  else if (dtype == "Cloud")
    parent_node->add_port<hmap::Cloud>(gnode::PortType::OUT, port_label);
  else if (dtype == "Path")
    parent_node->add_port<hmap::Path>(gnode::PortType::OUT, port_label);
  else if (dtype == "Forest")
    parent_node->add_port<hmap::Forest>(gnode::PortType::OUT, port_label);
  else if (dtype == "RockField")
    parent_node->add_port<hmap::RockField>(gnode::PortType::OUT, port_label);
  else
    parent_node->add_port<hmap::VirtualArray>(gnode::PortType::OUT,
                                              port_label,
                                              CONFIG((*this)));
}

void NodeOutNode::push_to_parent(gnode::Node *parent_node, const std::string &port_label)
{
  auto              *parent_base = dynamic_cast<BaseNode *>(parent_node);
  const GraphConfig &parent_cfg = parent_base ? parent_base->cfg() : this->cfg();
  const std::string  dtype = this->get_data_type_name();

  if (dtype == "VirtualArray")
  {
    auto *p_src = this->get_value_ref<hmap::VirtualArray>("value");
    auto *p_dst = parent_base
                      ? parent_base->get_value_ref<hmap::VirtualArray>(port_label)
                      : parent_node->get_value_ref<hmap::VirtualArray>(port_label);
    if (p_dst)
      transfer_virtual_array(p_src, this->cfg(), *p_dst, parent_cfg);
  }
  else if (dtype == "VirtualTexture")
  {
    auto *p_src = this->get_value_ref<hmap::VirtualTexture>("value");
    auto *p_dst = parent_base
                      ? parent_base->get_value_ref<hmap::VirtualTexture>(port_label)
                      : parent_node->get_value_ref<hmap::VirtualTexture>(port_label);
    if (p_dst)
      transfer_virtual_texture(p_src, this->cfg(), *p_dst, parent_cfg);
  }
  else if (dtype == "Array")
  {
    auto *p_src = this->get_value_ref<hmap::Array>("value");
    auto *p_dst = parent_node->get_value_ref<hmap::Array>(port_label);
    if (p_dst)
    {
      if (!p_src || p_src->size() == 0)
        *p_dst = hmap::Array(parent_cfg.shape);
      else if (p_src->shape == parent_cfg.shape)
        *p_dst = *p_src;
      else
        *p_dst = p_src->resample_to_shape_bilinear(parent_cfg.shape);
    }
  }
  else if (dtype == "Cloud")
  {
    auto *p_src = this->get_value_ref<hmap::Cloud>("value");
    auto *p_dst = parent_node->get_value_ref<hmap::Cloud>(port_label);
    if (p_dst)
      *p_dst = p_src ? *p_src : hmap::Cloud();
  }
  else if (dtype == "Path")
  {
    auto *p_src = this->get_value_ref<hmap::Path>("value");
    auto *p_dst = parent_node->get_value_ref<hmap::Path>(port_label);
    if (p_dst)
      *p_dst = p_src ? *p_src : hmap::Path();
  }
  else if (dtype == "Forest")
  {
    auto *p_src = this->get_value_ref<hmap::Forest>("value");
    auto *p_dst = parent_node->get_value_ref<hmap::Forest>(port_label);
    if (p_dst)
      *p_dst = p_src ? *p_src : hmap::Forest();
  }
  else if (dtype == "RockField")
  {
    auto *p_src = this->get_value_ref<hmap::RockField>("value");
    auto *p_dst = parent_node->get_value_ref<hmap::RockField>(port_label);
    if (p_dst)
      *p_dst = p_src ? *p_src : hmap::RockField();
  }
}

void setup_node_out_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  node.set_current_category("Boundary");
  add_string(node, "slot_name", "Port Name", "output_0");
  add_choice(node, "data_type", "Data Type", boundary_data_types(), "VirtualArray");

  if (auto *p_out = dynamic_cast<NodeOutNode *>(&node))
    p_out->rebuild_port();
  else
    node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "value");
}

void compute_node_out_node(BaseNode & /* node */)
{
  // Data is pushed to parent SubGraphNode by SubGraphNode::compute_subgraph()
}

} // namespace hesiod
