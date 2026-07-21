/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_cloud_to_path_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Cloud>(gnode::PortType::IN, "cloud");
  node.add_port<hmap::Path>(gnode::PortType::OUT, "path");

  // attribute(s)
  node.add_attr<BoolAttribute>("closed", "closed", false);
  node.add_attr<BoolAttribute>("reorder_nns", "reorder_nns", false);

  // attribute(s) order
  node.set_attr_ordered_key({"closed", "reorder_nns"});
}

void compute_cloud_to_path_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Cloud *p_in = node.get_value_ref<hmap::Cloud>("cloud");
  hmap::Path  *p_out = node.get_value_ref<hmap::Path>("path");

  if (!p_in)
    return;

  // convert the input
  *p_out = hmap::Path(p_in->points);

  p_out->set_closed(node.get_attr<BoolAttribute>("closed"));

  if (node.get_attr<BoolAttribute>("reorder_nns"))
    p_out->reorder_nns();
}

} // namespace hesiod
