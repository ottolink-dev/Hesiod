/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
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

constexpr const char *P_ROCK_FIELD = "rock_field";
constexpr const char *P_HEIGHTMAP  = "heightmap";
constexpr const char *P_ROCK_MAP   = "rock_map";

constexpr const char *A_SCATTER_SHAPE       = "scatter_shape";
constexpr const char *A_HEIGHT_RADIUS_RATIO = "height_radius_ratio";
constexpr const char *A_SEED                = "seed";

void setup_rocks_to_heightmap_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::RockField>(gnode::PortType::IN, P_ROCK_FIELD);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_HEIGHTMAP, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_ROCK_MAP, CONFIG(node));

  // --- Attributes

  add_enum(node, A_SCATTER_SHAPE, "Shape", enum_mappings.scatter_shape_map, "Polygon");
  add_float(node, A_HEIGHT_RADIUS_RATIO, "Height/Radius Ratio", 2.0f, 0.1f, 10.0f);
  add_seed(node, A_SEED, "Seed");

  setup_post_process_heightmap_attributes(
      node,
      {.add_mix = false, .remap_active_state = false});
}

void compute_rocks_to_heightmap_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  hmap::RockField    *p_in_rocks   = node.get_value_ref<hmap::RockField>(P_ROCK_FIELD);
  hmap::VirtualArray *p_out_height = node.get_value_ref<hmap::VirtualArray>(P_HEIGHTMAP);
  hmap::VirtualArray *p_out_rock_map = node.get_value_ref<hmap::VirtualArray>(P_ROCK_MAP);

  if (!p_in_rocks)
    return;

  // --- Parameters

  const auto shape_type = static_cast<hmap::ScatterShape>(node.val<int>(A_SCATTER_SHAPE));
  const auto height_radius_ratio = node.val<float>(A_HEIGHT_RADIUS_RATIO);
  const auto seed                = static_cast<uint32_t>(node.val<int>(A_SEED));

  // --- Convert RockField to Heightmap & Rock Map

  glm::ivec2 shape = node.cfg().shape;

  if (p_out_height || p_out_rock_map)
  {
    hmap::Array  rock_map;
    hmap::Array *p_rock_map_target = p_out_rock_map ? &rock_map : nullptr;

    hmap::Array rock_elevation = p_in_rocks->to_heightmap(shape,
                                                          shape_type,
                                                          height_radius_ratio,
                                                          std::nullopt,
                                                          p_rock_map_target,
                                                          seed);

    if (p_out_height)
    {
      p_out_height->from_array(rock_elevation, node.cfg().cm_cpu);
      post_process_heightmap(node, *p_out_height);
    }

    if (p_out_rock_map)
      p_out_rock_map->from_array(rock_map, node.cfg().cm_cpu);
  }
}

} // namespace hesiod
