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
constexpr const char *P_IN          = "input";
constexpr const char *P_WATER_DEPTH = "water_depth";

constexpr const char *A_ELEVATION  = "elevation";
constexpr const char *A_FROM_EAST  = "from_east";
constexpr const char *A_FROM_NORTH = "from_north";
constexpr const char *A_FROM_SOUTH = "from_south";
constexpr const char *A_FROM_WEST  = "from_west";

void setup_flooding_from_boundaries_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_WATER_DEPTH, CONFIG(node));

  // attribute(s)
  add_float(node, A_ELEVATION, "elevation", 0.2f, -1.f, 2.f);
  add_bool(node, A_FROM_EAST, "from_east", true);
  add_bool(node, A_FROM_WEST, "from_west", true);
  add_bool(node, A_FROM_NORTH, "from_north", true);
  add_bool(node, A_FROM_SOUTH, "from_south", true);
}

void compute_flooding_from_boundaries_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_WATER_DEPTH);

    hmap::for_each_tile(
        {p_out, p_in},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out              = hmap::flooding_from_boundaries(*pa_in,
                                                   node.val<float>(A_ELEVATION),
                                                   node.val<bool>(A_FROM_EAST),
                                                   node.val<bool>(A_FROM_WEST),
                                                   node.val<bool>(A_FROM_NORTH),
                                                   node.val<bool>(A_FROM_SOUTH));
        },
        node.cfg().cm_single_array); // forced, not tileable
  }
}

} // namespace hesiod
