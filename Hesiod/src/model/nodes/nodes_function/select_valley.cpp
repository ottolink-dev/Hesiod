/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/morphology.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/selector.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_select_valley_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("radius", "radius", 0.05f, 0.001f, 0.5f);
  node.add_attr<BoolAttribute>("ridge_select", "ridge_select", false);

  // attribute(s) order
  node.set_attr_ordered_key(
      {"_GROUPBOX_BEGIN_Main Parameters", "radius", "ridge_select", "_GROUPBOX_END_"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_select_valley_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in  = node.get_value_ref<hmap::VirtualArray>("input");
  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

  if (!p_in)
    return;

  int ir = std::max(1, (int)(node.get_attr<FloatAttribute>("radius") * p_out->shape.x));

  hmap::for_each_tile(
      {p_out, p_in},
      [&node, ir](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
      {
        auto [pa_out, pa_in] = unpack<2>(p_arrays);
        *pa_out              = hmap::gpu::select_valley(*pa_in,
                                           ir,
                                           node.get_attr<BoolAttribute>("ridge_select"));
      },
      node.cfg().cm_gpu);

  p_out->smooth_overlap_buffers();

  // post-process
  post_apply_saturate_percentile(node, *p_out, 0.f, 0.95f);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
