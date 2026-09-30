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

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_flooding_lake_system_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ELEVATION);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_WATER_DEPTH, CONFIG(node));

  // --- Attributes

  add_float(node, A_MININAL_RADIUS, "mininal_radius", 0., 0.f, 0.5f);
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_flooding_lake_system_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto *p_in  = node.get_value_ref<hmap::VirtualArray>(P_ELEVATION);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_WATER_DEPTH);

  if (!p_in)
    return;

  int   ir                = node.val_pixel_radius(A_MININAL_RADIUS, 0);
  float surface_threshold = M_PI * ir * ir;

  *p_out = hmap::va::flooding_lake_system(*p_in, surface_threshold, node.cfg().cm_cpu);
}

} // namespace hesiod
