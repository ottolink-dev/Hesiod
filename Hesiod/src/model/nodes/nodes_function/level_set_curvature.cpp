/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/curvature.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/range.hpp"

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

constexpr const char *A_VALUES_KEPT = "values_kept";
constexpr const char *A_RADIUS      = "radius";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_level_set_curvature_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  std::vector<std::string> choices = {"positive", "negative", "both"};

  // clang-format off
  add_choice(node, A_VALUES_KEPT, "Values Kept", choices, "both");
  add_float(node, A_RADIUS, "Radius", 0.1f, 0.f, 1.f, "{:.4f}");
  // clang-format on

  // --- Attribute(s) order

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_level_set_curvature_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in  = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in)
    return;

  // --- Params

  // clang-format off
  const auto radius      = node.val<float>(A_RADIUS);
  const auto values_kept = node.val<std::string>(A_VALUES_KEPT);
  // clang-format on

  bool keep_both = (values_kept == "both");
  int  ir        = std::max(1, int(radius * p_out->shape.x));

  // --- Compute

  hmap::for_each_tile(
      {p_in},
      {p_out},
      [&](std::vector<const hmap::Array *> in,
          std::vector<hmap::Array *>       out,
          const hmap::TileRegion &)
      {
        auto [pa_in]  = unpack<1>(in);
        auto [pa_out] = unpack<1>(out);

        *pa_out = hmap::gpu::level_set_curvature(*pa_in, ir);

        // keep only one curvature sign if requested
        if (!keep_both)
        {
          if (values_kept == "negative")
            *pa_out *= -1.f;

          hmap::clamp_min(*pa_out, 0.f);
        }
      },
      node.cfg().cm_cpu);

  // --- Post-process

  p_out->smooth_overlap_buffers();
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
