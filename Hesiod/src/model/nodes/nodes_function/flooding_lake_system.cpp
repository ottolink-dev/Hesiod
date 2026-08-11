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
constexpr const char *P_ELEVATION   = "elevation";
constexpr const char *P_WATER_DEPTH = "water_depth";

constexpr const char *A_MININAL_RADIUS = "mininal_radius";

void setup_flooding_lake_system_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ELEVATION);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_WATER_DEPTH, CONFIG(node));

  // attribute(s)
  add_float(node, A_MININAL_RADIUS, "mininal_radius", 0.05f, 0.f, 0.5f);
}

void compute_flooding_lake_system_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_ELEVATION);

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_WATER_DEPTH);

    int   ir                = node.val<float>(A_MININAL_RADIUS) * (float)p_in->shape.x;
    float surface_threshold = M_PI * ir * ir;

    hmap::for_each_tile(
        {p_out, p_in},
        [&node, surface_threshold](std::vector<hmap::Array *> p_arrays,
                                   const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out              = hmap::flooding_lake_system(*pa_in, surface_threshold);
        },
        node.cfg().cm_single_array); // forced, not tileable
  }
}

} // namespace hesiod
