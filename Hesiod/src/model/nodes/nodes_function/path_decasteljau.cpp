/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/geometry/path.hpp"

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

constexpr const char *A_EDGE_DIVISIONS = "edge_divisions";

void setup_path_decasteljau_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Path>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::Path>(gnode::PortType::OUT, P_OUT);

  // attribute(s)
  add_int(node, A_EDGE_DIVISIONS, "edge_divisions", 10, 1, 32);
}

void compute_path_decasteljau_node(BaseNode &node)
{
  Logger::log()->error("PathDecasteljau node is deprecated, use PathResample node");

  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Path *p_in = node.get_value_ref<hmap::Path>(P_IN);

  if (p_in)
  {
    hmap::Path *p_out = node.get_value_ref<hmap::Path>(P_OUT);

    if (p_in->size() > 1)
      *p_out = hmap::decasteljau(*p_in, node.val<int>(A_EDGE_DIVISIONS));
  }
}

} // namespace hesiod
