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
constexpr const char *P_A    = "A";
constexpr const char *P_B    = "B";
constexpr const char *P_G    = "G";
constexpr const char *P_R    = "R";
constexpr const char *P_RGBA = "RGBA";

void setup_heightmap_to_rgba_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_R);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_G);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_B);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_A);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT, P_RGBA, CONFIG_TEX(node));
}

void compute_heightmap_to_rgba_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_r = node.get_value_ref<hmap::VirtualArray>(P_R);
  hmap::VirtualArray *p_g = node.get_value_ref<hmap::VirtualArray>(P_G);
  hmap::VirtualArray *p_b = node.get_value_ref<hmap::VirtualArray>(P_B);
  hmap::VirtualArray *p_a = node.get_value_ref<hmap::VirtualArray>(P_A);

  if (p_r || p_g || p_b || p_a)
  {
    hmap::VirtualTexture *p_tex = node.get_value_ref<hmap::VirtualTexture>(P_RGBA);

    // clear texture
    for (int nch = 0; nch < p_tex->channels(); ++nch)
    {
      float v = nch == 3 ? 1.f : 0.f;
      p_tex->fill(nch, v, node.cfg().cm_cpu);
    }

    if (p_r)
      p_tex->channel(0).copy_from(*p_r, node.cfg().cm_cpu);
    if (p_g)
      p_tex->channel(1).copy_from(*p_g, node.cfg().cm_cpu);
    if (p_b)
      p_tex->channel(2).copy_from(*p_b, node.cfg().cm_cpu);
    if (p_a)
      p_tex->channel(3).copy_from(*p_a, node.cfg().cm_cpu);
  }
}

} // namespace hesiod
