/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "output";

constexpr const char *A_K_SMOOTHING = "k_smoothing";
constexpr const char *A_RADIUS      = "radius";
constexpr const char *A_VMAX        = "vmax";

void setup_hydraulic_blur_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_RADIUS, "radius", 0.1f, 0.01f, 0.5f);
  add_float(node, A_VMAX, "vmax", 0.5f, -1.f, 2.f);
  add_float(node, A_K_SMOOTHING, "k_smoothing", 0.1f, 0.f, 1.f);

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

void compute_hydraulic_blur_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto *p_in  = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in)
    return;

  float zmin = p_in->min(node.cfg().cm_cpu);
  float zmax = p_in->max(node.cfg().cm_cpu);

  hmap::for_each_tile(
      {p_out, p_in},
      [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
      {
        auto [pa_out, pa_in] = unpack<2>(p_arrays);
        *pa_out              = *pa_in;

        hmap::hydraulic_blur(*pa_out,
                             node.val<float>(A_RADIUS),
                             node.val<float>(A_VMAX),
                             node.val<float>(A_K_SMOOTHING));
      },
      node.cfg().cm_cpu);

  // preverse input elevation range
  p_out->remap(zmin, zmax, node.cfg().cm_cpu);

  // post-process
  p_out->smooth_overlap_buffers();
  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
