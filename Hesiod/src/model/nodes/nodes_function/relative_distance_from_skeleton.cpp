/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/morphology.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_relative_distance_from_skeleton_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("search_radius", "search_radius", 0.2f, 0.f, 0.5f);
  node.add_attr<FloatAttribute>("threshold", "threshold", 0.f, -1.f, 1.f);

  // attribute(s) order
  node.set_attr_ordered_key({"search_radius", "threshold"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

void compute_relative_distance_from_skeleton_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    int ir = std::max(
        1,
        (int)(node.get_attr<FloatAttribute>("search_radius") * p_out->shape.x));

    hmap::for_each_tile(
        {p_out, p_in},
        [&node, ir](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out = *pa_in;

          float threshold = node.get_attr<FloatAttribute>("threshold");
          if (threshold)
          {
            hmap::make_binary(*pa_out, threshold);
            *pa_out -= threshold;
          }

          *pa_out = hmap::gpu::relative_distance_from_skeleton(*pa_out, ir);
        },
        node.cfg().cm_gpu);

    p_out->smooth_overlap_buffers();

    // post-process
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
