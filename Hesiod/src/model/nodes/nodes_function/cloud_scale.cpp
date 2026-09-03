/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/geometry/cloud.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "output";

constexpr const char *A_CENTER = "center";
constexpr const char *A_SCALE  = "scale";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_cloud_scale_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Cloud>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::Cloud>(gnode::PortType::OUT, P_OUT);

  // attribute(s)
  add_float(node, A_SCALE, "Scale", 1.f, 0.01f, 1.5f);
  add_xy(node, A_CENTER, "Center", {0.5f, 0.5f}, 0.f, 1.f, 0.f, 1.f);
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_cloud_scale_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Cloud *p_in = node.get_value_ref<hmap::Cloud>(P_IN);

  if (!p_in)
    return;

  hmap::Cloud *p_out = node.get_value_ref<hmap::Cloud>(P_OUT);

  const auto scale  = node.val<float>(A_SCALE);
  const auto center = node.val<glm::vec2>(A_CENTER);

  *p_out = hmap::scale(*p_in, scale, center);
}

} // namespace hesiod
