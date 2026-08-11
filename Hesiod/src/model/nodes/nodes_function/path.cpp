/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/geometry/path.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_BACKGROUND = "background";
constexpr const char *P_OUT        = "path";

constexpr const char *A_PATH   = "path";
constexpr const char *A_CLOSED = "closed";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_path_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_BACKGROUND);
  node.add_port<hmap::Path>(gnode::PortType::OUT, P_OUT);

  // --- Attributes

  add_path(node, A_PATH, "Path");
  add_bool(node, A_CLOSED, "Closed Path", false);

  setup_background_image_for_cloud_attribute(node, A_PATH, P_BACKGROUND);
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_path_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_out = node.get_value_ref<hmap::Path>(P_OUT);

  // --- Params

  const auto path_attr = node.val<std::vector<glm::vec3>>(A_PATH);
  const auto closed    = node.val<bool>(A_CLOSED);

  // --- Compute

  *p_out = hmap::Path(path_attr);
  p_out->set_closed(closed);
}

} // namespace hesiod
