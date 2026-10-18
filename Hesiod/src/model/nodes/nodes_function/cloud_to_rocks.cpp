/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/geometry/cloud.hpp"
#include "highmap/rocks/rock_field.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_CLOUD      = "cloud";
constexpr const char *P_ROCK_FIELD = "rock_field";

constexpr const char *A_CLASS_ID       = "class_id";
constexpr const char *A_DEFAULT_RADIUS = "default_radius";

void setup_cloud_to_rocks_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::Cloud>(gnode::PortType::IN, P_CLOUD);
  node.add_port<hmap::RockField>(gnode::PortType::OUT, P_ROCK_FIELD);

  // --- Attributes

  add_int(node, A_CLASS_ID, "Class ID", 0, 0, INT_MAX);
  add_float(node, A_DEFAULT_RADIUS, "Default Radius", 0.005f, 1e-5f, 1.0f, "{:.4f}");
}

void compute_cloud_to_rocks_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Cloud     *p_in  = node.get_value_ref<hmap::Cloud>(P_CLOUD);
  hmap::RockField *p_out = node.get_value_ref<hmap::RockField>(P_ROCK_FIELD);

  if (!p_in || !p_out)
    return;

  const auto class_id = static_cast<uint32_t>(std::max(0, node.val<int>(A_CLASS_ID)));
  const auto default_radius = node.val<float>(A_DEFAULT_RADIUS);

  *p_out = hmap::RockField(*p_in, class_id, default_radius);
}

} // namespace hesiod
