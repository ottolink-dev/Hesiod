/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/curvature.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/range.hpp"

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

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "mask";

constexpr const char *A_RADIUS   = "radius";
constexpr const char *A_CTYPE    = "ctype";
constexpr const char *A_CLAMPING = "clamping";
constexpr const char *A_SATMAX   = "satmax";
constexpr const char *A_APPROX   = "approx";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_curvatures_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  std::vector<std::string> choices = {"Positive", "Negative", "Both"};

  // clang-format off
  add_float(node, A_RADIUS, "Radius", 0.f, 0.f, 0.5f);
  add_enum(node, A_CTYPE, "Curvature Type", enum_mappings.curvature_type_map, "Mean");
  add_choice(node, A_CLAMPING, "Values Kept", choices);
  add_float(node, A_SATMAX, "Saturation Ratio", 2.f, 0.f, 20.f, "{:.0f}%");
  add_bool(node, A_APPROX, "Approx. Algo.", false);
  // clang-format on

  // --- Attribute(s) order

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_curvatures_node(BaseNode &node)
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
  const auto ctype       = hmap::CurvatureType(node.val<int>(A_CTYPE));
  const auto clamping    = node.val<std::string>(A_CLAMPING);
  const auto approx_algo = node.val<bool>(A_APPROX);
  const auto sat_perc    = 0.01f * node.val<float>(A_SATMAX);
  // clang-format on

  const bool  keep_both = (clamping == "Both");
  const int   ir        = std::max(1, int(radius * p_out->shape.x));
  const float satmin    = sat_perc;
  const float satmax    = 1.f - sat_perc;

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

        *pa_out = hmap::gpu::curvature_quadric(*pa_in, ir, ctype, approx_algo);

        // keep only one curvature sign if requested
        if (!keep_both)
        {
          if (clamping == "Negative")
            *pa_out *= -1.f;

          hmap::clamp_min(*pa_out, 0.f);
        }
      },
      node.cfg().cm_gpu);

  // --- Post-process

  post_apply_saturate_percentile(node, *p_out, satmin, satmax);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
