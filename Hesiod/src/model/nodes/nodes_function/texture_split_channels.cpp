/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_A       = "A";
constexpr const char *P_B       = "B";
constexpr const char *P_G       = "G";
constexpr const char *P_R       = "R";
constexpr const char *P_TEXTURE = "texture";

void setup_texture_split_channels_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_TEXTURE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_R, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_G, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_B, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_A, CONFIG(node));

  // attribute(s)
}

void compute_texture_split_channels_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualTexture *p_tex = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE);

  if (p_tex)
  {
    hmap::VirtualArray *p_r = node.get_value_ref<hmap::VirtualArray>(P_R);
    hmap::VirtualArray *p_g = node.get_value_ref<hmap::VirtualArray>(P_G);
    hmap::VirtualArray *p_b = node.get_value_ref<hmap::VirtualArray>(P_B);
    hmap::VirtualArray *p_a = node.get_value_ref<hmap::VirtualArray>(P_A);

    p_r->copy_from(p_tex->channel(0), node.cfg().cm_cpu);
    p_g->copy_from(p_tex->channel(1), node.cfg().cm_cpu);
    p_b->copy_from(p_tex->channel(2), node.cfg().cm_cpu);
    p_a->copy_from(p_tex->channel(3), node.cfg().cm_cpu);
  }
}

} // namespace hesiod
