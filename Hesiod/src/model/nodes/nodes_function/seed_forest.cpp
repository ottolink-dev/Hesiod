/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/flora/forest_seeding.hpp"
#include "highmap/flora/species.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_ELEVATION         = "elevation";
constexpr const char *P_SECONDARY_DENSITY = "secondary_density";
constexpr const char *P_EXCLUSION_MASK    = "exclusion_mask";
constexpr const char *P_FOREST            = "forest";
constexpr const char *P_DENSITY           = "density";

constexpr const char *A_TREE_COUNT            = "tree_count";
constexpr const char *A_SPECIES_COUNT         = "species_count";
constexpr const char *A_CLUSTER_SPREAD        = "cluster_spread";
constexpr const char *A_POINTS_PER_CLUSTER    = "points_per_cluster";
constexpr const char *A_SEED                  = "seed";
constexpr const char *A_MIN_ELEV              = "min_elev";
constexpr const char *A_MAX_ELEV              = "max_elev";
constexpr const char *A_ELEV_TRANSITION_WIDTH = "elev_transition_width";
constexpr const char *A_WEIGHT_ELEV           = "weight_elev";
constexpr const char *A_MIN_SLOPE             = "min_slope";
constexpr const char *A_MAX_SLOPE             = "max_slope";
constexpr const char *A_WEIGHT_TALUS          = "weight_talus";
constexpr const char *A_ANGLE                 = "angle";
constexpr const char *A_ANGLE_WIDTH           = "angle_width";
constexpr const char *A_WEIGHT_ANGLE          = "weight_angle";
constexpr const char *A_WEIGHT_TWI            = "weight_twi";
constexpr const char *A_WEIGHT_SECONDARY      = "weight_secondary";
constexpr const char *A_EXCLUSION_THRESHOLD   = "exclusion_threshold";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_seed_forest_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ELEVATION);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_SECONDARY_DENSITY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_EXCLUSION_MASK);
  node.add_port<hmap::Forest>(gnode::PortType::OUT, P_FOREST);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_DENSITY, CONFIG(node));

  // --- Attributes

  // clang-format off
  node.set_current_category("Seeding");
  add_int(node, A_TREE_COUNT, "Tree Count", 1000, 1, 1000000);
  add_int(node, A_SPECIES_COUNT, "Species Count", 4, 1, 16);
  add_float(node, A_CLUSTER_SPREAD, "Cluster Spread", 0.05f, 0.001f, 0.5f);
  add_int(node, A_POINTS_PER_CLUSTER, "Points per Cluster", 8, 1, 64);
  add_seed(node, A_SEED, "Seed");

  node.set_current_category("Weights");
  add_float(node, A_WEIGHT_ELEV, "Elevation", 1.0f, 0.0f, 1.0f);
  add_float(node, A_WEIGHT_TALUS, "Slope", 1.0f, 0.0f, 1.0f);
  add_float(node, A_WEIGHT_ANGLE, "Aspect Angle", 0.f, 0.0f, 1.0f);
  add_float(node, A_WEIGHT_TWI, "TWI", 0.f, 0.0f, 1.0f);
  add_float(node, A_WEIGHT_SECONDARY, "Secondary", 1.0f, 0.0f, 1.0f);

  node.set_current_category("Elevation Criterion");
  add_float(node, A_MIN_ELEV, "Min Elevation", 0.0f, -1.0f, 2.0f);
  add_float(node, A_MAX_ELEV, "Max Elevation", 0.8f, -1.0f, 2.0f);
  add_float(node, A_ELEV_TRANSITION_WIDTH, "Transition Width", 0.1f, 0.0f, 1.0f);

  node.set_current_category("Slope Criterion");
  add_float(node, A_MIN_SLOPE, "Min Slope", 1.0f, 0.0f, 16.0f);
  add_float(node, A_MAX_SLOPE, "Max Slope", 4.0f, 0.0f, 16.0f);

  node.set_current_category("Aspect Angle Criterion");
  add_angle(node, A_ANGLE, "Aspect Angle", 30.0f, 0.0f, 360.0f);
  add_float(node, A_ANGLE_WIDTH, "Angle Tolerance", 90.0f, 0.0f, 180.0f);

  node.set_current_category("Modulation & Exclusion");
  add_float(node, A_EXCLUSION_THRESHOLD, "Exclusion Threshold", 0.5f, 0.0f, 1.0f);
  // clang-format on
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_seed_forest_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_elevation   = node.get_value_ref<hmap::VirtualArray>(P_ELEVATION);
  auto *p_secondary   = node.get_value_ref<hmap::VirtualArray>(P_SECONDARY_DENSITY);
  auto *p_exclusion   = node.get_value_ref<hmap::VirtualArray>(P_EXCLUSION_MASK);
  auto *p_out_forest  = node.get_value_ref<hmap::Forest>(P_FOREST);
  auto *p_out_density = node.get_value_ref<hmap::VirtualArray>(P_DENSITY);

  if (!p_elevation)
    return;

  // --- Params

  // clang-format off
  const auto tree_count            = static_cast<size_t>(std::max(1, node.val<int>(A_TREE_COUNT)));
  const auto species_count         = static_cast<size_t>(std::max(1, node.val<int>(A_SPECIES_COUNT)));
  const auto cluster_spread        = node.val<float>(A_CLUSTER_SPREAD);
  const auto points_per_cluster    = static_cast<size_t>(std::max(1, node.val<int>(A_POINTS_PER_CLUSTER)));
  const auto seed                  = static_cast<uint32_t>(node.val<int>(A_SEED));
  const auto min_elev              = node.val<float>(A_MIN_ELEV);
  const auto max_elev              = node.val<float>(A_MAX_ELEV);
  const auto elev_transition_width = node.val<float>(A_ELEV_TRANSITION_WIDTH);
  const auto weight_elev           = node.val<float>(A_WEIGHT_ELEV);
  const auto min_slope             = node.val<float>(A_MIN_SLOPE);
  const auto max_slope             = node.val<float>(A_MAX_SLOPE);
  const auto weight_talus          = node.val<float>(A_WEIGHT_TALUS);
  const auto angle                 = node.val<float>(A_ANGLE);
  const auto angle_width           = node.val<float>(A_ANGLE_WIDTH);
  const auto weight_angle          = node.val<float>(A_WEIGHT_ANGLE);
  const auto weight_twi            = node.val<float>(A_WEIGHT_TWI);
  const auto weight_secondary      = node.val<float>(A_WEIGHT_SECONDARY);
  const auto exclusion_threshold   = node.val<float>(A_EXCLUSION_THRESHOLD);
  // clang-format on

  // --- Convert inputs to full arrays

  hmap::Array elev_array = p_elevation->to_array(node.cfg().cm_cpu);

  hmap::Array secondary_array;
  if (p_secondary)
    secondary_array = p_secondary->to_array(node.cfg().cm_cpu);

  hmap::Array exclusion_array;
  if (p_exclusion)
    exclusion_array = p_exclusion->to_array(node.cfg().cm_cpu);

  // --- Talus Computation from Slope

  const float shape_x   = std::max(1.0f, static_cast<float>(elev_array.shape.x));
  const float min_talus = min_slope / shape_x;
  const float max_talus = max_slope / shape_x;

  // --- Build Tree Density Map

  hmap::Array density_array = hmap::build_tree_density(elev_array,
                                                       min_elev,
                                                       max_elev,
                                                       elev_transition_width,
                                                       min_talus,
                                                       max_talus,
                                                       angle,
                                                       angle_width,
                                                       weight_elev,
                                                       weight_talus,
                                                       weight_angle,
                                                       weight_twi,
                                                       secondary_array,
                                                       weight_secondary,
                                                       exclusion_array);

  if (p_out_density)
    p_out_density->from_array(density_array, node.cfg().cm_cpu);

  // --- Seed Forest

  if (p_out_forest)
  {
    hmap::ForestSeedingOptions options;
    options.bbox                = {0.0f, 1.0f, 0.0f, 1.0f};
    options.seed                = seed;
    options.exclusion_threshold = exclusion_threshold;

    *p_out_forest = hmap::seed_forest_clusters(species_count,
                                               tree_count,
                                               density_array,
                                               exclusion_array,
                                               cluster_spread,
                                               points_per_cluster,
                                               options);

    p_out_forest->set_elevation_from_terrain(elev_array);
  }
}

} // namespace hesiod
