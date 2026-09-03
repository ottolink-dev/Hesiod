/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/hydrology/hydrology.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_ELEVATION       = "elevation";
constexpr const char *P_WATER_DEPTH     = "water_depth";
constexpr const char *P_WATER_ELEVATION = "water_elevation";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_water_elevation_from_depth_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ELEVATION);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_WATER_DEPTH);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT,
                                    P_WATER_ELEVATION,
                                    CONFIG(node));

  // attribute(s)
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_water_elevation_from_depth_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_z     = node.get_value_ref<hmap::VirtualArray>(P_ELEVATION);
  auto *p_depth = node.get_value_ref<hmap::VirtualArray>(P_WATER_DEPTH);
  auto *p_wz    = node.get_value_ref<hmap::VirtualArray>(P_WATER_ELEVATION);

  if (!p_z || !p_depth)
    return;

  // --- Compute

  hmap::for_each_tile(
      {p_wz, p_depth, p_z},
      [&](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
      {
        auto [pa_wz, pa_depth, pa_z] = unpack<3>(p_arrays);

        *pa_wz = *pa_z + *pa_depth;
      },
      node.cfg().cm_cpu);
}

} // namespace hesiod
