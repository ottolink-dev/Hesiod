/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/geometry/path.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_path_smooth_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Path>(gnode::PortType::IN, "input");
  node.add_port<hmap::Path>(gnode::PortType::OUT, "output");

  // attribute(s)
  node.add_attr<IntAttribute>("navg", "navg", 1, 1, 10);
  node.add_attr<FloatAttribute>("averaging_intensity",
                                "averaging_intensity",
                                1.f,
                                0.f,
                                1.f);
  node.add_attr<FloatAttribute>("inertia", "inertia", 0.f, 0.f, 1.f);

  // attribute(s) order
  node.set_attr_ordered_key({"navg", "averaging_intensity", "inertia"});
}

void compute_path_smooth_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Path *p_in = node.get_value_ref<hmap::Path>("input");

  if (p_in)
  {
    hmap::Path *p_out = node.get_value_ref<hmap::Path>("output");

    if (p_in->size() > 1)
      *p_out = hmap::smooth(*p_in,
                            node.get_attr<IntAttribute>("navg"),
                            node.get_attr<FloatAttribute>("averaging_intensity"),
                            node.get_attr<FloatAttribute>("inertia"));
  }
}

} // namespace hesiod
