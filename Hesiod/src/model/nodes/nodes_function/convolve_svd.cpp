/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/convolve.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_convolve_svd_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::Array>(gnode::PortType::IN, "kernel");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<IntAttribute>("rank", "rank", 4, 1, 8);

  // attribute(s) order
  node.set_attr_ordered_key({"rank"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

void compute_convolve_svd_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in     = node.get_value_ref<hmap::VirtualArray>("input");
  hmap::Array        *p_kernel = node.get_value_ref<hmap::Array>("kernel");

  if (p_in && p_kernel)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    hmap::for_each_tile(
        {p_out, p_in},
        [&node, p_kernel](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out              = hmap::convolve2d_svd(*pa_in,
                                         *p_kernel,
                                         node.get_attr<IntAttribute>("rank"));
        },
        node.cfg().cm_cpu);

    p_out->smooth_overlap_buffers();

    // post-process
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
