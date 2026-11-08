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
constexpr const char *P_CLOUD       = "cloud";
constexpr const char *P_ELEVATION   = "elevation";
constexpr const char *P_WATER_DEPTH = "water_depth";

constexpr const char *A_DEPTH_MIN                   = "depth_min";
constexpr const char *A_USE_DISTRIBUTED_COMPUTATION = "use_distributed_computation";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_flooding_from_point_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ELEVATION);
  node.add_port<hmap::Cloud>(gnode::PortType::IN, P_CLOUD);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_WATER_DEPTH, CONFIG(node));

  // --- Attributes

  add_float(node, A_DEPTH_MIN, "depth_min", 0.01f, 0.f, 1.f);

  // --- Advanced parameters

  node.set_current_category("Advanced");
  add_bool(node, A_USE_DISTRIBUTED_COMPUTATION, "Use Distributed Algorithm", false);
}

void compute_flooding_from_point_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  hmap::VirtualArray *p_in    = node.get_value_ref<hmap::VirtualArray>(P_ELEVATION);
  hmap::Cloud        *p_cloud = node.get_value_ref<hmap::Cloud>(P_CLOUD);
  hmap::VirtualArray *p_out   = node.get_value_ref<hmap::VirtualArray>(P_WATER_DEPTH);

  if (!p_in || !p_cloud)
    return;

  // --- Params

  // clang-format off
  const auto depth_min                   = node.val<float>(A_DEPTH_MIN);
  const auto use_distributed_computation = node.val<bool>(A_USE_DISTRIBUTED_COMPUTATION);
  // clang-format on

  const auto cm = use_distributed_computation ? node.cfg().cm_cpu
                                              : node.cfg().cm_single_array;

  // convert point positions to cell indices (assuming unit square bbox)
  std::vector<int> i, j;

  {
    const glm::ivec2 &shape = node.cfg().shape;

    for (const auto &p : *p_cloud)
    {
      int ip = static_cast<int>(p.x * (shape.x - 1));
      int jp = static_cast<int>(p.y * (shape.y - 1));

      if (ip >= 0 && ip <= shape.x - 1 && jp >= 0 && jp <= shape.y - 1)
      {
        i.push_back(ip);
        j.push_back(jp);
      }
    }
  }

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

        *pa_out = hmap::flooding_from_point(*pa_in, i, j, depth_min, region, comm);
      },
      cm);
}

} // namespace hesiod
