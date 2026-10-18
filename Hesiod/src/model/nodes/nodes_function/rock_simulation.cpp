/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/rocks/rock_simulation.hpp"
#include "highmap/rocks/rock_distribution.hpp"
#include "highmap/rocks/rock_field.hpp"
#include "highmap/scatter/scatter_field.hpp"
#include "highmap/virtual_array/virtual_array.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_ELEVATION  = "elevation";
constexpr const char *P_DENSITY    = "density";
constexpr const char *P_EXCLUSION  = "exclusion";
constexpr const char *P_ROCK_FIELD = "rock_field";
constexpr const char *P_HEIGHTMAP  = "heightmap";
constexpr const char *P_ROCK_MAP   = "rock_map";

constexpr const char *A_ROCK_COUNT             = "rock_count";
constexpr const char *A_TIME_STEP              = "time_step";
constexpr const char *A_MAX_STEPS              = "max_steps";
constexpr const char *A_GRAVITY                = "gravity";
constexpr const char *A_SOIL_FRICTION          = "soil_friction";
constexpr const char *A_ROLLING_RESISTANCE     = "rolling_resistance";
constexpr const char *A_INTER_ROCK_RESTITUTION = "inter_rock_restitution";
constexpr const char *A_MIN_VELOCITY           = "min_velocity";
constexpr const char *A_RESPAWN_OUT_OF_BOUNDS  = "respawn_out_of_bounds";
constexpr const char *A_SPAWN_FRACTION         = "spawn_fraction";
constexpr const char *A_SEED                   = "seed";

constexpr const char *A_RADIUS_MIN      = "radius_min";
constexpr const char *A_RADIUS_MAX      = "radius_max";
constexpr const char *A_POWER_LAW_ALPHA = "power_law_alpha";

constexpr const char *A_RESOLVE_COLLISIONS = "resolve_collisions";
constexpr const char *A_PRUNE_COLLISIONS   = "prune_collisions";

constexpr const char *A_SCATTER_SHAPE       = "scatter_shape";
constexpr const char *A_HEIGHT_RADIUS_RATIO = "height_radius_ratio";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_rock_simulation_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ELEVATION);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DENSITY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_EXCLUSION);
  node.add_port<hmap::RockField>(gnode::PortType::OUT, P_ROCK_FIELD);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_HEIGHTMAP, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_ROCK_MAP, CONFIG(node));

  // --- Attributes

  // clang-format off
  node.set_current_category("Emission & Simulation");
  add_int(node, A_ROCK_COUNT, "Rock Count", 20000, 1, 500000);
  add_int(node, A_MAX_STEPS, "Max Steps", 1024, 16, INT_MAX);
  add_float(node, A_SOIL_FRICTION, "Soil Friction", 0.55f, 0.01f, 2.0f);
  add_float(node, A_ROLLING_RESISTANCE, "Rolling Resistance", 0.05f, 0.001f, 1.0f);
  add_float(node, A_INTER_ROCK_RESTITUTION, "Restitution", 0.9f, 0.0f, 1.0f);
  add_float(node, A_SPAWN_FRACTION, "Spawn Fraction", 0.8f, 0.0f, 1.0f);
  add_bool(node, A_RESPAWN_OUT_OF_BOUNDS, "Respawn Out of Bounds", true);
  add_seed(node, A_SEED, "Seed");

  node.set_current_category("Size Distribution");
  add_float(node, A_RADIUS_MIN, "Min Radius", 0.0005f, 0.0001f, 0.05f, "{:.4f}");
  add_float(node, A_RADIUS_MAX, "Max Radius", 0.002f, 0.0001f, 0.1f, "{:.4f}");
  add_float(node, A_POWER_LAW_ALPHA, "Power Law Alpha", 4.0f, 1.0f, 10.0f);

  node.set_current_category("Collision Resolution");
  add_bool(node, A_RESOLVE_COLLISIONS, "Resolve Collisions", true);
  add_bool(node, A_PRUNE_COLLISIONS, "Prune Collisions", true);

  node.set_current_category("Heightmap Generation");
  add_enum(node, A_SCATTER_SHAPE, "Shape", enum_mappings.scatter_shape_map, "Polygon");
  add_float(node, A_HEIGHT_RADIUS_RATIO, "Height/Radius Ratio", 2.0f, 0.1f, 10.0f);

  node.set_current_category("Advanced");
  add_float(node, A_TIME_STEP, "Time Step", 0.005f, 0.0001f, 0.05f, "{:.4f}");
  add_float(node, A_GRAVITY, "Gravity", 9.81f, 0.1f, 50.0f);
  add_float(node, A_MIN_VELOCITY, "Min Velocity", 0.01f, 0.0f, 1.0f);
  // clang-format on

  setup_post_process_heightmap_attributes(
      node,
      {.add_mix = false, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_rock_simulation_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_elevation    = node.get_value_ref<hmap::VirtualArray>(P_ELEVATION);
  auto *p_density      = node.get_value_ref<hmap::VirtualArray>(P_DENSITY);
  auto *p_exclusion    = node.get_value_ref<hmap::VirtualArray>(P_EXCLUSION);
  auto *p_out_rocks    = node.get_value_ref<hmap::RockField>(P_ROCK_FIELD);
  auto *p_out_height   = node.get_value_ref<hmap::VirtualArray>(P_HEIGHTMAP);
  auto *p_out_rock_map = node.get_value_ref<hmap::VirtualArray>(P_ROCK_MAP);

  if (!p_elevation)
    return;

  // --- Parameters

  const auto rock_count = static_cast<size_t>(std::max(1, node.val<int>(A_ROCK_COUNT)));
  const auto time_step  = node.val<float>(A_TIME_STEP);
  const auto max_steps  = std::max(1, node.val<int>(A_MAX_STEPS));
  const auto gravity    = node.val<float>(A_GRAVITY);
  const auto soil_friction          = node.val<float>(A_SOIL_FRICTION);
  const auto rolling_resistance     = node.val<float>(A_ROLLING_RESISTANCE);
  const auto inter_rock_restitution = node.val<float>(A_INTER_ROCK_RESTITUTION);
  const auto min_velocity           = node.val<float>(A_MIN_VELOCITY);
  const auto spawn_fraction         = node.val<float>(A_SPAWN_FRACTION);
  const auto respawn_out_of_bounds  = node.val<bool>(A_RESPAWN_OUT_OF_BOUNDS);
  const auto seed                   = static_cast<uint32_t>(node.val<int>(A_SEED));

  const auto radius_min      = node.val<float>(A_RADIUS_MIN);
  const auto radius_max      = node.val<float>(A_RADIUS_MAX);
  const auto power_law_alpha = node.val<float>(A_POWER_LAW_ALPHA);

  const auto do_resolve = node.val<bool>(A_RESOLVE_COLLISIONS);
  const auto do_prune   = node.val<bool>(A_PRUNE_COLLISIONS);

  const auto shape_type = static_cast<hmap::ScatterShape>(node.val<int>(A_SCATTER_SHAPE));
  const auto height_radius_ratio = node.val<float>(A_HEIGHT_RADIUS_RATIO);

  // --- Terrain Elevation Array

  hmap::Array elev_array = p_elevation->to_array(node.cfg().cm_cpu);
  glm::ivec2  shape      = elev_array.shape;

  // --- Emission Density Array

  hmap::Array emission_density;

  if (p_density)
    emission_density = p_density->to_array(node.cfg().cm_cpu);
  else
    emission_density = hmap::Array(shape, 1.0f);

  // Apply exclusion mask if provided
  if (p_exclusion)
  {
    hmap::Array exclusion_array = p_exclusion->to_array(node.cfg().cm_cpu);
    emission_density            = emission_density * (1.0f - exclusion_array);
    hmap::clamp(emission_density, 0.0f, 1.0f);
  }

  // --- Configure Simulation Options & Distribution

  hmap::RockDistribution dist(0u, radius_min, radius_max, power_law_alpha);

  hmap::RockSimulationOptions options;
  options.bbox                   = {0.0f, 1.0f, 0.0f, 1.0f};
  options.time_step              = time_step;
  options.max_steps              = max_steps;
  options.gravity                = gravity;
  options.soil_friction          = soil_friction;
  options.rolling_resistance     = rolling_resistance;
  options.inter_rock_restitution = inter_rock_restitution;
  options.min_velocity           = min_velocity;
  options.spawn_fraction         = spawn_fraction;
  options.respawn_out_of_bounds  = respawn_out_of_bounds;
  options.seed                   = seed;

  // --- Simulate Rock Emission on GPU

  hmap::RockField rocks = hmap::simulate_rock_emission(rock_count,
                                                       emission_density,
                                                       elev_array,
                                                       nullptr,
                                                       dist,
                                                       options);

  if (do_resolve)
    rocks.resolve_collisions();

  if (do_prune)
    rocks.prune_collisions();

  // Store output RockField
  if (p_out_rocks)
    *p_out_rocks = rocks;

  // --- Convert RockField to Heightmap & Rock Map

  if (p_out_height || p_out_rock_map)
  {
    hmap::Array  rock_map;
    hmap::Array *p_rock_map_target = p_out_rock_map ? &rock_map : nullptr;

    hmap::Array rock_elevation = rocks.to_heightmap(shape,
                                                    shape_type,
                                                    height_radius_ratio,
                                                    std::nullopt,
                                                    p_rock_map_target,
                                                    seed);

    if (p_out_height)
    {
      hmap::Array total_elevation = elev_array + rock_elevation;
      p_out_height->from_array(total_elevation, node.cfg().cm_cpu);
      post_process_heightmap(node, *p_out_height);
    }

    if (p_out_rock_map)
      p_out_rock_map->from_array(rock_map, node.cfg().cm_cpu);
  }
}

} // namespace hesiod
