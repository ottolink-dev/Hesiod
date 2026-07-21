/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/hydrology/hydrology.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_merge_water_depths_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "depth1");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "depth2");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "water_depth", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("k_smooth", "k_smooth", 0.f, 0.f, 0.1f, "{:.4f}");

  // attribute(s) order
  node.set_attr_ordered_key({"k_smooth"});
}

void compute_merge_water_depths_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in1 = node.get_value_ref<hmap::VirtualArray>("depth1");
  hmap::VirtualArray *p_in2 = node.get_value_ref<hmap::VirtualArray>("depth2");

  if (p_in1 && p_in2)
  {
    hmap::VirtualArray *p_depth = node.get_value_ref<hmap::VirtualArray>("water_depth");

    hmap::for_each_tile(
        {p_depth, p_in1, p_in2},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          hmap::Array *pa_depth = p_arrays[0];
          hmap::Array *pa_in1 = p_arrays[1];
          hmap::Array *pa_in2 = p_arrays[2];

          *pa_depth = hmap::merge_water_depths(*pa_in1,
                                               *pa_in2,
                                               node.get_attr<FloatAttribute>("k_smooth"));
        },
        node.cfg().cm_cpu);

    p_depth->smooth_overlap_buffers();
  }
}

} // namespace hesiod
