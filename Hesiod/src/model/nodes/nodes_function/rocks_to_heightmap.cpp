/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/scatter/scatter_field.hpp"
#include "highmap/virtual_array/virtual_array.hpp"
#include "highmap/world/rocks/rock_field.hpp"

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

  hmap::for_each_tile(
      {},
      {p_out_height, p_out_rock_map},
      [&](std::vector<const hmap::Array *>,
          std::vector<hmap::Array *> p_arrays_out,
          const hmap::UvTile        &region)
      {
        auto [pa_height, pa_rock_map] = unpack<2>(p_arrays_out);

        hmap::Array rock_elevation = p_in_rocks->to_heightmap(region.shape,
                                                              shape_type,
                                                              height_radius_ratio,
                                                              std::nullopt,
                                                              pa_rock_map,
                                                              seed,
                                                              region.uv_rect);

        if (pa_height)
          *pa_height = std::move(rock_elevation);
      },
      node.cfg().cm_cpu);

  post_process_heightmap(node, *p_out_height);
}

} // namespace hesiod
