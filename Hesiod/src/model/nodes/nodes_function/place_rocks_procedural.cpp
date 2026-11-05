/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/rocks/rock_distribution.hpp"
#include "highmap/rocks/rock_field.hpp"
#include "highmap/rocks/rock_procedural.hpp"
#include "highmap/rocks/rock_seeding.hpp"
#include "highmap/virtual_array/virtual_array.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_ELEVATION         = "elevation";
constexpr const char *P_SECONDARY_DENSITY = "secondary_density";
constexpr const char *P_EXCLUSION_MASK    = "exclusion_mask";
constexpr const char *P_ROCK_FIELD        = "rock_field";
constexpr const char *P_DENSITY           = "density";

constexpr const char *A_ROCK_COUNT = "rock_count";
constexpr const char *A_MIN_ELEV   = "min_elev";
constexpr const char *A_MAX_ELEV   = "max_elev";
constexpr const char *A_MAX_SLOPE  = "max_slope";
constexpr const char *A_SEED       = "seed";

constexpr const char *A_BULKIFY_AMP          = "bulkify_amp";
constexpr const char *A_WEIGHT_ELEV          = "weight_elev";
constexpr const char *A_WEIGHT_SLOPE         = "weight_slope";
constexpr const char *A_WEIGHT_RIVERS        = "weight_rivers";
constexpr const char *A_RIVER_TALUS_REF      = "river_talus_ref";
constexpr const char *A_RIVER_CLIPPING_RATIO = "river_clipping_ratio";
constexpr const char *A_WEIGHT_SECONDARY     = "weight_secondary";

constexpr const char *A_CLASS_ID        = "class_id";
constexpr const char *A_RADIUS_MIN      = "radius_min";
constexpr const char *A_RADIUS_MAX      = "radius_max";
constexpr const char *A_POWER_LAW_ALPHA = "power_law_alpha";

constexpr const char *A_SIZE_SLOPE_WEIGHT       = "size_slope_weight";
constexpr const char *A_SIZE_ELEVATION_GAIN     = "size_elevation_gain";
constexpr const char *A_SIZE_ELEVATION_EXPONENT = "size_elevation_exponent";

constexpr const char *A_EXCLUSION_THRESHOLD = "exclusion_threshold";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_place_rocks_procedural_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ELEVATION);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_SECONDARY_DENSITY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_EXCLUSION_MASK);
  node.add_port<hmap::RockField>(gnode::PortType::OUT, P_ROCK_FIELD);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_DENSITY, CONFIG(node));

  // --- Attributes

  // clang-format off
  node.set_current_category("Seeding & Placement");
  add_int(node, A_ROCK_COUNT, "Rock Count", 10000, 1, 1000000);
  add_float(node, A_MIN_ELEV, "Min Elevation", 0.0f, -2.0f, 2.0f);
  add_float(node, A_MAX_ELEV, "Max Elevation", 0.9f, -2.0f, 2.0f);
  add_float(node, A_MAX_SLOPE, "Max Slope", 4.0f, 0.0f, 16.0f);
  add_seed(node, A_SEED, "Seed");

  node.set_current_category("Density Weights");
  add_float(node, A_BULKIFY_AMP, "Bulkify Amplitude", 0.01f, 0.0f, 1.0f);
  add_float(node, A_WEIGHT_ELEV, "Elevation Weight", 1.0f, 0.0f, 1.0f);
  add_float(node, A_WEIGHT_SLOPE, "Slope Weight", 1.0f, 0.0f, 1.0f);
  add_float(node, A_WEIGHT_RIVERS, "Rivers Weight", 1.0f, 0.0f, 1.0f);
  add_float(node, A_RIVER_TALUS_REF, "River Talus Ref", 0.1f, 0.001f, 2.0f);
  add_float(node, A_RIVER_CLIPPING_RATIO, "River Clipping Ratio", 1000.0f, 1.0f, 10000.0f);
  add_float(node, A_WEIGHT_SECONDARY, "Secondary Weight", 1.0f, 0.0f, 1.0f);

  node.set_current_category("Size Distribution");
  add_int(node, A_CLASS_ID, "Class ID", 0, 0, INT_MAX);
  add_float(node, A_RADIUS_MIN, "Min Radius", 0.001f, 0.0001f, 0.1f, "{:.4f}");
  add_float(node, A_RADIUS_MAX, "Max Radius", 0.05f, 0.0001f, 1.0f, "{:.4f}");
  add_float(node, A_POWER_LAW_ALPHA, "Power Law Alpha", 2.0f, 0.1f, 10.0f);

  node.set_current_category("Size Attenuation");
  add_float(node, A_SIZE_SLOPE_WEIGHT, "Slope Attenuation", 0.0f, 0.0f, 1.0f);
  add_float(node, A_SIZE_ELEVATION_GAIN, "Elevation Gain", 1.0f, 0.0f, 10.0f);
  add_float(node, A_SIZE_ELEVATION_EXPONENT, "Elevation Exponent", 1.0f, 0.1f, 10.0f);

  node.set_current_category("Exclusion");
  add_float(node, A_EXCLUSION_THRESHOLD, "Exclusion Threshold", 0.0f, 0.0f, 1.0f);
  // clang-format on
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_place_rocks_procedural_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_elevation   = node.get_value_ref<hmap::VirtualArray>(P_ELEVATION);
  auto *p_secondary   = node.get_value_ref<hmap::VirtualArray>(P_SECONDARY_DENSITY);
  auto *p_exclusion   = node.get_value_ref<hmap::VirtualArray>(P_EXCLUSION_MASK);
  auto *p_out_rocks   = node.get_value_ref<hmap::RockField>(P_ROCK_FIELD);
  auto *p_out_density = node.get_value_ref<hmap::VirtualArray>(P_DENSITY);

  if (!p_elevation || !p_out_rocks)
    return;

  // --- Parameters

  const auto rock_count = static_cast<size_t>(std::max(1, node.val<int>(A_ROCK_COUNT)));
  const auto min_elev   = node.val<float>(A_MIN_ELEV);
  const auto max_elev   = node.val<float>(A_MAX_ELEV);
  const auto max_slope  = node.val<float>(A_MAX_SLOPE);
  const auto seed       = static_cast<uint32_t>(node.val<int>(A_SEED));

  const auto bulkify_amp          = node.val<float>(A_BULKIFY_AMP);
  const auto weight_elev          = node.val<float>(A_WEIGHT_ELEV);
  const auto weight_slope         = node.val<float>(A_WEIGHT_SLOPE);
  const auto weight_rivers        = node.val<float>(A_WEIGHT_RIVERS);
  const auto river_talus_ref      = node.val<float>(A_RIVER_TALUS_REF);
  const auto river_clipping_ratio = node.val<float>(A_RIVER_CLIPPING_RATIO);
  const auto weight_secondary     = node.val<float>(A_WEIGHT_SECONDARY);

  const auto class_id   = static_cast<uint32_t>(std::max(0, node.val<int>(A_CLASS_ID)));
  const auto radius_min = node.val<float>(A_RADIUS_MIN);
  const auto radius_max = node.val<float>(A_RADIUS_MAX);
  const auto power_law_alpha = node.val<float>(A_POWER_LAW_ALPHA);

  const auto size_slope_weight       = node.val<float>(A_SIZE_SLOPE_WEIGHT);
  const auto size_elevation_gain     = node.val<float>(A_SIZE_ELEVATION_GAIN);
  const auto size_elevation_exponent = node.val<float>(A_SIZE_ELEVATION_EXPONENT);

  const auto exclusion_threshold = node.val<float>(A_EXCLUSION_THRESHOLD);

  // --- Talus Computation from Slope

  const float shape_x   = std::max(1.0f, static_cast<float>(p_elevation->shape.x));
  const float max_talus = max_slope / shape_x;

  // --- Build Rock Density Map

  hmap::VirtualArray  local_density;
  hmap::VirtualArray *p_density_va = p_out_density;

  if (!p_density_va)
  {
    local_density = hmap::VirtualArray(CONFIG(node));
    p_density_va  = &local_density;
  }

  hmap::for_each_tile(
      {p_elevation, p_secondary, p_exclusion},
      {p_density_va},
      [&](std::vector<const hmap::Array *> p_arrays_in,
          std::vector<hmap::Array *>       p_arrays_out,
          const hmap::TileRegion          &region)
      {
        auto [pa_elevation, pa_secondary, pa_exclusion] = unpack<3>(p_arrays_in);
        auto [pa_density]                               = unpack<1>(p_arrays_out);

        static const hmap::Array empty_array;
        const auto              &secondary = pa_secondary ? *pa_secondary : empty_array;
        const auto              &exclusion = pa_exclusion ? *pa_exclusion : empty_array;

        *pa_density = hmap::build_rock_density(*pa_elevation,
                                               max_talus,
                                               bulkify_amp,
                                               weight_elev,
                                               weight_slope,
                                               weight_rivers,
                                               river_talus_ref,
                                               river_clipping_ratio,
                                               secondary,
                                               weight_secondary,
                                               exclusion,
                                               exclusion_threshold,
                                               region.bbox);
      },
      node.cfg().cm_cpu);

  // --- Configure Seeding Options

  hmap::RockSeedingOptions options;
  options.bbox                = p_elevation->bbox;
  options.seed                = seed;
  options.exclusion_threshold = exclusion_threshold;
  options.distribution        = hmap::RockDistribution(class_id,
                                                radius_min,
                                                radius_max,
                                                power_law_alpha);

  const hmap::VirtualArray empty_va{};
  const auto              &exclusion_ref = p_exclusion ? *p_exclusion : empty_va;

  // --- Run Procedural Placement

  *p_out_rocks = hmap::va::place_rocks_procedural(rock_count,
                                                  *p_elevation,
                                                  min_elev,
                                                  max_elev,
                                                  max_talus,
                                                  *p_density_va,
                                                  exclusion_ref,
                                                  size_slope_weight,
                                                  size_elevation_gain,
                                                  size_elevation_exponent,
                                                  options,
                                                  node.cfg().cm_cpu);

  p_out_rocks->prune_collisions();
}

} // namespace hesiod
