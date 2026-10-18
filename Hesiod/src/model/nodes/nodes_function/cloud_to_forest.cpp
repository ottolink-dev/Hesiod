/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/flora/forest.hpp"
#include "highmap/geometry/cloud.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_CLOUD  = "cloud";
constexpr const char *P_FOREST = "forest";

constexpr const char *A_SPECIES_ID     = "species_id";
constexpr const char *A_DEFAULT_RADIUS = "default_radius";

void setup_cloud_to_forest_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::Cloud>(gnode::PortType::IN, P_CLOUD);
  node.add_port<hmap::Forest>(gnode::PortType::OUT, P_FOREST);

  // --- Attributes

  add_int(node, A_SPECIES_ID, "Species ID", 0, 0, INT_MAX);
  add_float(node, A_DEFAULT_RADIUS, "Default Radius", 0.001f, 1e-5f, 1.0f, "{:.4f}");
}

void compute_cloud_to_forest_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Cloud  *p_in  = node.get_value_ref<hmap::Cloud>(P_CLOUD);
  hmap::Forest *p_out = node.get_value_ref<hmap::Forest>(P_FOREST);

  if (!p_in || !p_out)
    return;

  const auto species_id = static_cast<uint32_t>(std::max(0, node.val<int>(A_SPECIES_ID)));
  const auto default_radius = node.val<float>(A_DEFAULT_RADIUS);

  *p_out = hmap::Forest(*p_in, species_id, default_radius);
}

} // namespace hesiod
