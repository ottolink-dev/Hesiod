/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/morphology.hpp"
#include "highmap/opencl/gpu_opencl.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
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

constexpr const char *A_RADIUS    = "radius";
constexpr const char *A_OPERATOR  = "operator";
constexpr const char *A_SAT_RATIO = "sat_ratio";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_morphological_operators_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  // clang-format off
  add_float(node, A_RADIUS, "radius", 0.01f, 0.f, 0.2f);
  add_enum(node, A_OPERATOR, "Operator", enum_mappings.morphology_operation_map, "Gradient");
  add_float(node, A_SAT_RATIO, "Saturation Ratio", 2.f, 0.f, 20.f, "{:.0f}%");
  // clang-format on

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_morphological_operators_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto *p_in  = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in)
    return;

  // --- Params

  // clang-format off
  const auto radius = node.val<float>(A_RADIUS);
  const auto op = hmap::MorphologyOperation(node.val<int>(A_OPERATOR));
  const auto sat_ratio = node.val<float>(A_SAT_RATIO);
  //
  const int  ir     = std::max(1, (int)(radius * p_out->shape.x));
  const float satmax = (1.f - 0.01f * sat_ratio);
  // clang-format on

  // --- Compute

  hmap::for_each_tile(
      {p_out, p_in},
      [&](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
      {
        auto [pa_out, pa_in] = unpack<2>(p_arrays);
        *pa_out              = hmap::gpu::morphological_operators(*pa_in, ir, op);
      },
      node.cfg().cm_gpu);

  // post-process
  p_out->smooth_overlap_buffers();
  post_apply_saturate_percentile(node, *p_out, 0.f, satmax);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
