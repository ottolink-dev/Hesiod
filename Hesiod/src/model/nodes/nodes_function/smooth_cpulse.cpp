/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/opencl/gpu_opencl.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_INPUT = "input";
constexpr const char *P_MASK = "mask";
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
  node.add_attr<FloatAttribute>(A_RADIUS, "Radius", 0.05f, 0.f, 0.2f);

  // attribute(s) order
  node.set_attr_ordered_key({A_RADIUS});

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

  auto *p_in = node.get_value_ref<hmap::VirtualArray>(P_INPUT);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUTPUT);

  if (!p_in || !p_out)
    return;

  auto *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);

  // prepare mask
  std::shared_ptr<hmap::VirtualArray> sp_mask = pre_process_mask(node, p_mask, *p_in);

  int ir = std::max(1, (int)(node.get_attr<FloatAttribute>(A_RADIUS) * p_out->shape.x));

  hmap::for_each_tile(
      {p_in, p_mask},
      {p_out},
      [&ir](std::vector<const hmap::Array *> p_arrays_in,
            std::vector<hmap::Array *>       p_arrays_out,
            const hmap::TileRegion &)
      {
        const auto [pa_in, pa_mask] = unpack<2>(p_arrays_in);
        auto [pa_out] = unpack<1>(p_arrays_out);
        *pa_out = *pa_in;

        hmap::gpu::smooth_cpulse(*pa_out, ir, pa_mask);
      },
      node.cfg().cm_gpu);

  p_out->smooth_overlap_buffers();

  // post-process
  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
