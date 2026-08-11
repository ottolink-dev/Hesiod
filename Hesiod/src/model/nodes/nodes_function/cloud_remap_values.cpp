/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "output";

constexpr const char *A_REMAP = "remap";

void setup_cloud_remap_values_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Cloud>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::Cloud>(gnode::PortType::OUT, P_OUT);

  // attribute(s)
  add_range(node, A_REMAP, "remap", {0.f, 1.f}, -1.f, 2.f, true);
}

void compute_cloud_remap_values_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Cloud *p_in = node.get_value_ref<hmap::Cloud>(P_IN);

  if (p_in)
  {
    hmap::Cloud *p_out = node.get_value_ref<hmap::Cloud>(P_OUT);

    // copy and remap the input
    *p_out = *p_in;
    p_out->remap_values(node.val<glm::vec2>(A_REMAP)[0], node.val<glm::vec2>(A_REMAP)[1]);
  }
}

} // namespace hesiod
