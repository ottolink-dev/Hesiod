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

constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "mask";

constexpr const char *A_RADIUS    = "radius";
constexpr const char *A_CTYPE     = "ctype";
constexpr const char *A_CLAMPING  = "clamping";
constexpr const char *A_APPROX    = "approx";
constexpr const char *A_NORMALIZE = "normalize";

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
  add_bool(node, A_NORMALIZE, "Normalize", true);
  add_bool(node, A_APPROX, "Approx. Algo.", false);
  // clang-format on

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
  const auto ctype       = node.val_enum<hmap::CurvatureType>(A_CTYPE);
  const auto clamping    = node.val<std::string>(A_CLAMPING);
  const auto normalize   = node.val<bool>(A_NORMALIZE);
  const auto approx_algo = node.val<bool>(A_APPROX);
  // clang-format on

  const bool keep_both = (clamping == "Both");
  const int  ir        = node.val_pixel_radius(A_RADIUS);

  // --- Compute

  hmap::for_each_tile(
      {p_in},
      {p_out},
      [&](std::vector<const hmap::Array *> p_arrays_in,
          std::vector<hmap::Array *>       p_arrays_out,
          const hmap::TileRegion &)
      {
        auto [pa_in]  = unpack<1>(p_arrays_in);
        auto [pa_out] = unpack<1>(p_arrays_out);

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

  if (normalize)
  {
    hmap::for_each_tile(
        {},
        {p_out},
        [&](std::vector<const hmap::Array *>,
            std::vector<hmap::Array *> p_arrays_out,
            const hmap::TileRegion &,
            hmap::ICommunicator &comm)
        {
          auto [pa_out] = unpack<1>(p_arrays_out);
          hmap::normalize_and_clip(*pa_out, 2.5f, true, true, comm);
        },
        node.cfg().cm_cpu);
  }

  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
