/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_texture_split_channels_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, "texture");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "R", CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "G", CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "B", CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "A", CONFIG(node));

  // attribute(s)

  // attribute(s) order
}

void compute_texture_split_channels_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualTexture *p_tex = node.get_value_ref<hmap::VirtualTexture>("texture");

  if (p_tex)
  {
    hmap::VirtualArray *p_r = node.get_value_ref<hmap::VirtualArray>("R");
    hmap::VirtualArray *p_g = node.get_value_ref<hmap::VirtualArray>("G");
    hmap::VirtualArray *p_b = node.get_value_ref<hmap::VirtualArray>("B");
    hmap::VirtualArray *p_a = node.get_value_ref<hmap::VirtualArray>("A");

    p_r->copy_from(p_tex->channel(0), node.cfg().cm_cpu);
    p_g->copy_from(p_tex->channel(1), node.cfg().cm_cpu);
    p_b->copy_from(p_tex->channel(2), node.cfg().cm_cpu);
    p_a->copy_from(p_tex->channel(3), node.cfg().cm_cpu);
  }
}

} // namespace hesiod
