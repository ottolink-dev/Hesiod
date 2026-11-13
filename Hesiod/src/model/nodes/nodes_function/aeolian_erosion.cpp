/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <cmath>

#include "highmap/erosion.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_BEDROCK_IN    = "bedrock_in";
constexpr const char *P_SAND_MAP      = "sand_map";
constexpr const char *P_VEGETATION    = "vegetation";
constexpr const char *P_ABRASION_MASK = "abrasion_mask";
constexpr const char *P_OUTPUT        = "output";
constexpr const char *P_SEDIMENT      = "sediment";
constexpr const char *P_BEDROCK_OUT   = "bedrock";

constexpr const char *A_STEPS             = "steps";
constexpr const char *A_SEED              = "seed";
constexpr const char *A_SAND_HEIGHT       = "sand_height";
constexpr const char *A_WIND_NORM         = "wind_norm";
constexpr const char *A_WIND_ANGLE        = "wind_angle";
constexpr const char *A_MATTER_TO_MOVE    = "matter_to_move";
constexpr const char *A_SAND_REPOSE_ANGLE = "sand_repose_angle";
constexpr const char *A_SHADOW_ANGLE_MIN  = "shadow_angle_min";
constexpr const char *A_SHADOW_ANGLE_MAX  = "shadow_angle_max";
constexpr const char *A_MAX_SHADOW_DIST   = "max_shadow_dist";
constexpr const char *A_ABRASION_RATE     = "abrasion_rate";
constexpr const char *A_REPOSE_RATE       = "repose_rate";
constexpr const char *A_LEVELS            = "levels";
constexpr const char *A_COARSE_FRACTION   = "coarse_fraction";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_aeolian_erosion_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_BEDROCK_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_SAND_MAP);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_VEGETATION);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ABRASION_MASK);

  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUTPUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_SEDIMENT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_BEDROCK_OUT, CONFIG(node));

  // --- Attributes

  // clang-format off
  node.set_current_category("Simulation");
  add_int(node, A_STEPS, "Steps", 300, 1, INT_MAX);
  add_seed(node, A_SEED, "Seed");
  add_float(node, A_SAND_HEIGHT, "Sand Height", 0.01f, 0.001f, 0.5f, "{:.2e}", /* log */ true);
  add_float(node, A_WIND_NORM, "Wind Intensity", 1.f, 0.f, 8.f);
  add_angle(node, A_WIND_ANGLE, "Wind Angle", 0.f, -180.f, 180.f);
  add_float(node, A_MATTER_TO_MOVE, "Matter to Move", 3e-4f, 1e-5f, 0.01f, "{:.2e}", /* log */ true);
  add_int(node, A_LEVELS, "Multiscale Levels", 3, 1, 8);
  add_float(node, A_COARSE_FRACTION, "Coarse Fraction", 0.7f, 0.f, 0.95f);

  node.set_current_category("Dune Dynamics");
  add_angle(node, A_SAND_REPOSE_ANGLE, "Repose Angle", 33.f, 0.f, 90.f);
  add_angle(node, A_SHADOW_ANGLE_MIN, "Shadow Angle Min", 5.f, 0.f, 90.f);
  add_angle(node, A_SHADOW_ANGLE_MAX, "Shadow Angle Max", 15.f, 0.f, 90.f);
  add_float(node, A_MAX_SHADOW_DIST, "Max Shadow Distance", 12.f, 0.f, 1000.f);

  node.set_current_category("Abrasion & Repose");
  add_float(node, A_ABRASION_RATE, "Abrasion Rate", 0.f, 0.f, 1.f, "{:.4f}");
  add_float(node, A_REPOSE_RATE, "Repose Rate", 0.35f, 0.f, 1.f);
  // clang-format on

  // --- Post-process
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_aeolian_erosion_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  const auto *p_bedrock       = node.get_value_ref<hmap::VirtualArray>(P_BEDROCK_IN);
  const auto *p_sand_map      = node.get_value_ref<hmap::VirtualArray>(P_SAND_MAP);
  const auto *p_vegetation    = node.get_value_ref<hmap::VirtualArray>(P_VEGETATION);
  const auto *p_abrasion_mask = node.get_value_ref<hmap::VirtualArray>(P_ABRASION_MASK);

  auto *p_out         = node.get_value_ref<hmap::VirtualArray>(P_OUTPUT);
  auto *p_sediment    = node.get_value_ref<hmap::VirtualArray>(P_SEDIMENT);
  auto *p_bedrock_out = node.get_value_ref<hmap::VirtualArray>(P_BEDROCK_OUT);

  if (!p_bedrock && !p_sand_map)
    return;

  // --- Params

  const int           steps           = node.val<int>(A_STEPS);
  const std::uint32_t seed            = static_cast<std::uint32_t>(node.val<int>(A_SEED));
  const float         sand_height     = node.val<float>(A_SAND_HEIGHT);
  const float         wind_norm       = node.val<float>(A_WIND_NORM);
  const float         wind_angle      = node.val<float>(A_WIND_ANGLE);
  const float         matter_to_move  = node.val<float>(A_MATTER_TO_MOVE);
  const int           levels          = node.val<int>(A_LEVELS);
  const float         coarse_fraction = node.val<float>(A_COARSE_FRACTION);
  const float         sand_repose_angle = node.val<float>(A_SAND_REPOSE_ANGLE);
  const float         shadow_angle_min  = node.val<float>(A_SHADOW_ANGLE_MIN);
  const float         shadow_angle_max  = node.val<float>(A_SHADOW_ANGLE_MAX);
  const float         max_shadow_dist   = node.val<float>(A_MAX_SHADOW_DIST);
  const float         abrasion_rate     = node.val<float>(A_ABRASION_RATE);
  const float         repose_rate       = node.val<float>(A_REPOSE_RATE);

  const float     wind_angle_rad = wind_angle * hmap::cte::deg_to_rad;
  const glm::vec2 wind           = wind_norm *
                         glm::vec2(std::cos(wind_angle_rad), std::sin(wind_angle_rad));

  // --- Compute

  hmap::for_each_tile(
      {p_bedrock, p_sand_map, p_vegetation, p_abrasion_mask},
      {p_out, p_sediment, p_bedrock_out},
      [&](std::vector<const hmap::Array *> p_arrays_in,
          std::vector<hmap::Array *>       p_arrays_out,
          const hmap::TileRegion          &region)
      {
        auto [pa_bedrock, pa_sand_map, pa_vegetation, pa_abrasion_mask] = unpack<4>(
            p_arrays_in);
        auto [pa_out, pa_sediment, pa_bedrock_out] = unpack<3>(p_arrays_out);

        *pa_out = hmap::Array(region.shape, 0.f);

        if (pa_bedrock)
          *pa_out += *pa_bedrock;

        if (pa_sand_map)
          *pa_out += (*pa_sand_map) * sand_height;
        else
          *pa_out += sand_height;

        hmap::aeolian_erosion_multiscale(*pa_out,
                                         steps,
                                         seed,
                                         wind,
                                         pa_bedrock,
                                         pa_vegetation,
                                         pa_abrasion_mask,
                                         pa_sediment,
                                         pa_bedrock_out,
                                         /* cell_size */ -1.f,
                                         matter_to_move,
                                         sand_repose_angle,
                                         shadow_angle_min,
                                         shadow_angle_max,
                                         max_shadow_dist,
                                         abrasion_rate,
                                         repose_rate,
                                         levels,
                                         coarse_fraction);
      },
      node.cfg().cm_gpu);

  p_out->sync_overlap_buffers();

  if (p_sediment)
    p_sediment->sync_overlap_buffers();

  if (p_bedrock_out)
    p_bedrock_out->sync_overlap_buffers();

  // --- Post-treatments

  post_process_heightmap(node, *p_out, p_bedrock ? p_bedrock : p_sand_map);
}

} // namespace hesiod
