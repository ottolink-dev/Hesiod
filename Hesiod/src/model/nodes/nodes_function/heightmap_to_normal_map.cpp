/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/gradient.hpp"
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
constexpr const char *P_IN         = "input";
constexpr const char *P_NORMAL_MAP = "normal map";

void setup_heightmap_to_normal_map_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT,
                                      P_NORMAL_MAP,
                                      CONFIG_TEX(node));

  // attribute(s)
}

void compute_heightmap_to_normal_map_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualTexture *p_nmap = node.get_value_ref<hmap::VirtualTexture>(P_NORMAL_MAP);

    hmap::Array  array = p_in->to_array(node.cfg().cm_cpu);
    hmap::Tensor tn    = hmap::normal_map(array);

    hmap::Array nx = tn.get_slice(0);
    hmap::Array ny = tn.get_slice(1);
    hmap::Array nz = tn.get_slice(2);

    for (int nch = 0; nch < 3; ++nch)
      p_nmap->channel(nch).from_array(tn.get_slice(nch), node.cfg().cm_cpu);

    p_nmap->fill(3, 1.f, node.cfg().cm_cpu);
  }
}

} // namespace hesiod
