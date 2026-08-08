/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include "highmap/colorize.hpp"
#include "highmap/geometry/cloud.hpp"
#include "highmap/operator.hpp"
#include "highmap/virtual_array/virtual_array.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_BACKGROUND = "background";
constexpr const char *P_OUT        = "cloud";

constexpr const char *A_CLOUD = "cloud";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_cloud_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_BACKGROUND);
  node.add_port<hmap::Cloud>(gnode::PortType::OUT, P_OUT);

  // --- Attributes

  node.set_current_category("Main");
  add_cloud(node, A_CLOUD, "Cloud");

  setup_background_image_for_cloud_attribute(node, A_CLOUD, P_BACKGROUND);
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_cloud_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_out = node.get_value_ref<hmap::Cloud>(P_OUT);

  if (!p_out)
    return;

  // --- Params

  const auto cloud_attr = node.val<std::vector<glm::vec3>>(A_CLOUD);

  // --- Compute

  *p_out = hmap::Cloud(cloud_attr);
}

} // namespace hesiod
