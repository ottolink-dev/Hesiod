/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/geometry/path.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_path_bezier_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Path>(gnode::PortType::IN, "input");
  node.add_port<hmap::Path>(gnode::PortType::OUT, "output");

  // attribute(s)
  node.add_attr<FloatAttribute>("curvature_ratio", "curvature_ratio", 0.3f, 0.f, 1.f);
  node.add_attr<IntAttribute>("edge_divisions", "edge_divisions", 10, 1, 32);

  // attribute(s) order
  node.set_attr_ordered_key({"curvature_ratio", "edge_divisions"});
}

void compute_path_bezier_node(BaseNode &node)
{
  Logger::log()->error("PathBezier node is deprecated, use PathResample node");

  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Path *p_in = node.get_value_ref<hmap::Path>("input");

  if (p_in)
  {
    hmap::Path *p_out = node.get_value_ref<hmap::Path>("output");

    if (p_in->size() > 1)
      *p_out = hmap::bezier(*p_in,
                            node.get_attr<FloatAttribute>("curvature_ratio"),
                            node.get_attr<IntAttribute>("edge_divisions"));
  }
}

} // namespace hesiod
