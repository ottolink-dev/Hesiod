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

constexpr const char *A_ELEVATION                   = "elevation";
constexpr const char *A_FROM_EAST                   = "from_east";
constexpr const char *A_FROM_NORTH                  = "from_north";
constexpr const char *A_FROM_SOUTH                  = "from_south";
constexpr const char *A_FROM_WEST                   = "from_west";
constexpr const char *A_USE_DISTRIBUTED_COMPUTATION = "use_distributed_computation";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_flooding_from_boundaries_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_WATER_DEPTH, CONFIG(node));

  // --- Attributes

  add_float(node, A_ELEVATION, "elevation", 0.2f, -1.f, 2.f);
  add_bool(node, A_FROM_EAST, "from_east", true);
  add_bool(node, A_FROM_WEST, "from_west", true);
  add_bool(node, A_FROM_NORTH, "from_north", true);
  add_bool(node, A_FROM_SOUTH, "from_south", true);

  // --- Advanced parameters

  node.set_current_category("Advanced");
  add_bool(node, A_USE_DISTRIBUTED_COMPUTATION, "Use Distributed Algorithm", false);
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_flooding_from_boundaries_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in  = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_WATER_DEPTH);

  if (!p_in)
    return;

  // --- Params

  // clang-format off
  const auto elevation                   = node.val<float>(A_ELEVATION);
  const auto from_east                   = node.val<bool>(A_FROM_EAST);
  const auto from_west                   = node.val<bool>(A_FROM_WEST);
  const auto from_north                  = node.val<bool>(A_FROM_NORTH);
  const auto from_south                  = node.val<bool>(A_FROM_SOUTH);
  const auto use_distributed_computation = node.val<bool>(A_USE_DISTRIBUTED_COMPUTATION);
  // clang-format on

  const auto cm = use_distributed_computation ? node.cfg().cm_cpu
                                              : node.cfg().cm_single_array;

  // --- Compute

  hmap::for_each_tile(
      {p_in},
      {p_out},
      [&](std::vector<const hmap::Array *> p_arrays_in,
          std::vector<hmap::Array *>       p_arrays_out,
          const hmap::TileRegion          &region,
          hmap::ICommunicator             &comm)
      {
        auto [pa_in]  = unpack<1>(p_arrays_in);
        auto [pa_out] = unpack<1>(p_arrays_out);

        *pa_out = hmap::flooding_from_boundaries(*pa_in,
                                                 elevation,
                                                 from_east,
                                                 from_west,
                                                 from_north,
                                                 from_south,
                                                 region,
                                                 comm);
      },
      cm);
}

} // namespace hesiod
