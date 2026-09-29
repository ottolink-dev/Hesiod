/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/opencl/gpu_opencl.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_INPUT  = "input";
constexpr const char *P_MASK   = "mask";
constexpr const char *P_OUTPUT = "output";

constexpr const char *A_RADIUS = "radius";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_smooth_cpulse_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_INPUT);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUTPUT, CONFIG(node));

  // attribute(s)
  node.set_current_category("Main Parameters");
  add_float(node, A_RADIUS, "Radius", 0.05f, 0.f, 0.2f);

  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_smooth_cpulse_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  const auto *p_in   = node.get_value_ref<hmap::VirtualArray>(P_INPUT);
  const auto *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  auto       *p_out  = node.get_value_ref<hmap::VirtualArray>(P_OUTPUT);

  if (!p_in || !p_out)
    return;

  // prepare mask
  hmap::VirtualArray mask_default = pre_process_mask(node, p_mask, *p_in);
  if (!mask_default.empty())
    p_mask = &mask_default;

  int ir = node.val_pixel_radius(A_RADIUS);

  *p_out = hmap::va::smooth_cpulse(*p_in, ir, p_mask, node.cfg().cm_gpu);

  // post-process
  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
