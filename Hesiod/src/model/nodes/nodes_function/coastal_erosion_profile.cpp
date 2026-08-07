/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"

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

constexpr const char *P_Z_IN     = "elevation_in";
constexpr const char *P_DEPTH_IN = "water_depth_in";
constexpr const char *P_NOISE    = "noise";
constexpr const char *P_MASK     = "mask";

constexpr const char *P_Z_OUT      = "elevation";
constexpr const char *P_DEPTH_OUT  = "water_depth";
constexpr const char *P_SHORE_MASK = "shore_mask";
constexpr const char *P_SCARP_MASK = "scarp_mask";

constexpr const char *A_GROUND_EXTENT          = "shore_ground_extent";
constexpr const char *A_WATER_RATIO            = "shore_water_extent_ratio";
constexpr const char *A_SCARP_RATIO            = "scarp_extent_ratio";
constexpr const char *A_SLOPE                  = "slope_shore";
constexpr const char *A_POST_FILTER            = "apply_post_filter";
constexpr const char *A_POST_FILTER_ITERATIONS = "post_filter_iterations";
constexpr const char *A_SOLID_SHORE_MASK       = "solid_shore_mask";
constexpr const char *A_SCARP_MASK_RATIO       = "scarp_mask_ratio";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_coastal_erosion_profile_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_Z_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DEPTH_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_NOISE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);

  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_Z_OUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_DEPTH_OUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_SHORE_MASK, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_SCARP_MASK, CONFIG(node));

  // --- Attributes

  // clang-format off
  node.add_attr<FloatAttribute>(A_GROUND_EXTENT, "Ground Shore Width", 0.03f, 0.f, 0.2f);
  node.add_attr<FloatAttribute>(A_WATER_RATIO, "Water Shore Width Ratio", 10.f, 0.f, 10.f);
  node.add_attr<FloatAttribute>(A_SCARP_RATIO, "Scarp Extent", 0.9f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_SLOPE, "Shore Slope", 0.5f, 0.f, 8.f);
  node.add_attr<BoolAttribute>(A_POST_FILTER, "Enable Shore Smoothing", true);
  node.add_attr<IntAttribute>(A_POST_FILTER_ITERATIONS, "Iterations", 10, 1, 32);
  node.add_attr<BoolAttribute>(A_SOLID_SHORE_MASK, "Solid Shore Mask", true);
  node.add_attr<FloatAttribute>(A_SCARP_MASK_RATIO, "Scarp Mask Transition", 0.2f, 0.f, 1.f);
  // clang-format on

  // --- Attribute(s) order

  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Shore Geometry",
                             A_GROUND_EXTENT,
                             A_WATER_RATIO,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Profile Shape",
                             A_SCARP_RATIO,
                             A_SLOPE,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Smoothing",
                             A_POST_FILTER,
                             A_POST_FILTER_ITERATIONS,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Output Masks",
                             A_SOLID_SHORE_MASK,
                             A_SCARP_MASK_RATIO,
                             "_GROUPBOX_END_"});

  setup_default_noise(node, {.noise_amp = 1.f, .kw = 8.f, .smoothness = 0.f});
  setup_pre_process_mask_attributes(node);
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_coastal_erosion_profile_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_z     = node.get_value_ref<hmap::VirtualArray>(P_Z_IN);
  auto *p_depth = node.get_value_ref<hmap::VirtualArray>(P_DEPTH_IN);
  auto *p_noise = node.get_value_ref<hmap::VirtualArray>(P_NOISE);
  auto *p_mask  = node.get_value_ref<hmap::VirtualArray>(P_MASK);

  auto *p_z_out      = node.get_value_ref<hmap::VirtualArray>(P_Z_OUT);
  auto *p_depth_out  = node.get_value_ref<hmap::VirtualArray>(P_DEPTH_OUT);
  auto *p_shore_mask = node.get_value_ref<hmap::VirtualArray>(P_SHORE_MASK);
  auto *p_scarp_mask = node.get_value_ref<hmap::VirtualArray>(P_SCARP_MASK);

  if (!p_z || !p_depth)
    return;

  // --- Params

  // clang-format off
  const auto ground_extent    = node.get_attr<FloatAttribute>(A_GROUND_EXTENT);
  const auto water_ratio      = node.get_attr<FloatAttribute>(A_WATER_RATIO);
  const auto scarp_ratio      = node.get_attr<FloatAttribute>(A_SCARP_RATIO);
  const auto slope            = node.get_attr<FloatAttribute>(A_SLOPE);
  const auto post_filter      = node.get_attr<BoolAttribute>(A_POST_FILTER);
  const auto iterations       = node.get_attr<IntAttribute>(A_POST_FILTER_ITERATIONS);
  const auto solid_shore_mask = node.get_attr<BoolAttribute>(A_SOLID_SHORE_MASK);
  const auto scarp_mask_ratio = node.get_attr<FloatAttribute>(A_SCARP_MASK_RATIO);
  // clang-format on

  int ir_ground = std::max(1, int(ground_extent * p_z->shape.x));
  int ir_water  = int(water_ratio * ir_ground);

  // --- Prepare mask

  std::shared_ptr<hmap::VirtualArray> sp_mask = pre_process_mask(node, p_mask, *p_z);

  // --- Resolve default noise

  hmap::VirtualArray noise_default(CONFIG(node));
  generate_noise(node, p_noise, noise_default);

  // --- Compute

  hmap::for_each_tile(
      {p_z, p_depth, p_noise, p_mask},
      {p_z_out, p_depth_out, p_shore_mask, p_scarp_mask},
      [&](std::vector<const hmap::Array *> in,
          std::vector<hmap::Array *>       out,
          const hmap::TileRegion &)
      {
        auto [pa_z, pa_depth, pa_noise, pa_mask]                    = unpack<4>(in);
        auto [pa_z_out, pa_depth_out, pa_shore_mask, pa_scarp_mask] = unpack<4>(out);

        *pa_z_out     = *pa_z;
        *pa_depth_out = *pa_depth;

        hmap::coastal_erosion_profile(*pa_z_out,
                                      pa_mask,
                                      *pa_depth_out,
                                      ir_ground,
                                      ir_water,
                                      slope,
                                      slope,
                                      scarp_ratio,
                                      post_filter,
                                      iterations,
                                      solid_shore_mask,
                                      scarp_mask_ratio,
                                      pa_noise,
                                      pa_shore_mask,
                                      pa_scarp_mask);
      },
      node.cfg().cm_cpu);

  // --- Post-process

  p_z_out->smooth_overlap_buffers();
  p_depth_out->smooth_overlap_buffers();
  p_shore_mask->smooth_overlap_buffers();
  p_scarp_mask->smooth_overlap_buffers();
}

} // namespace hesiod
