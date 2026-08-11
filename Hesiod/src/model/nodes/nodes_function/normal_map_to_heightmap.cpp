/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/gradient.hpp"
#include "highmap/range.hpp"
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
constexpr const char *P_NORMAL_MAP = "normal map";
constexpr const char *P_OUT        = "output";

constexpr const char *A_ITERATIONS     = "iterations";
constexpr const char *A_OMEGA          = "omega";
constexpr const char *A_POISSON_SOLVER = "poisson_solver";

void setup_normal_map_to_heightmap_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_NORMAL_MAP);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_bool(node, A_POISSON_SOLVER, "poisson_solver", false);
  add_int(node, A_ITERATIONS, "iterations", 500, 1, INT_MAX);
  add_float(node, A_OMEGA, "omega", 1.5f, 1e-3f, 2.f);

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

void compute_normal_map_to_heightmap_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualTexture *p_nmap = node.get_value_ref<hmap::VirtualTexture>(P_NORMAL_MAP);

  if (p_nmap)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    hmap::Tensor ts = hmap::Tensor(p_nmap->shape, 3);
    for (int nch = 0; nch < 3; ++nch)
      ts.set_slice(nch, p_nmap->channel(nch).to_array(node.cfg().cm_cpu));

    hmap::Array z;

    if (node.val<bool>(A_POISSON_SOLVER))
    {
      z = hmap::normal_map_to_heightmap_poisson(ts,
                                                node.val<int>(A_ITERATIONS),
                                                node.val<float>(A_OMEGA));
    }
    else
    {
      z = hmap::normal_map_to_heightmap(ts);
    }

    p_out->from_array(z, node.cfg().cm_cpu);
    p_out->remap(0.f, 1.f, node.cfg().cm_cpu);

    // post-process
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
