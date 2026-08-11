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
constexpr const char *P_CLOUD = "cloud";
constexpr const char *P_PATH  = "path";

constexpr const char *A_CLOSED      = "closed";
constexpr const char *A_REORDER_NNS = "reorder_nns";

void setup_cloud_to_path_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Cloud>(gnode::PortType::IN, P_CLOUD);
  node.add_port<hmap::Path>(gnode::PortType::OUT, P_PATH);

  // attribute(s)
  add_bool(node, A_CLOSED, "closed", false);
  add_bool(node, A_REORDER_NNS, "reorder_nns", false);
}

void compute_cloud_to_path_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Cloud *p_in  = node.get_value_ref<hmap::Cloud>(P_CLOUD);
  hmap::Path  *p_out = node.get_value_ref<hmap::Path>(P_PATH);

  if (!p_in)
    return;

  // convert the input
  *p_out = hmap::Path(p_in->points);

  p_out->set_closed(node.val<bool>(A_CLOSED));

  if (node.val<bool>(A_REORDER_NNS))
    p_out->reorder_nns();
}

} // namespace hesiod
