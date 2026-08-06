/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/curvature.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_unsphericity_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("radius", "radius", 0.01f, 0.f, 0.2f);
  node.add_attr<BoolAttribute>("remap", "remap", true);
  node.add_attr<BoolAttribute>("inverse", "inverse", false);
  node.add_attr<BoolAttribute>("smoothing", "smoothing", false);
  node.add_attr<FloatAttribute>("smoothing_radius", "smoothing_radius", 0.05f, 0.f, 0.2f);

  // attribute(s) order
  node.set_attr_ordered_key(
      {"radius", "inverse", "remap", "smoothing", "smoothing_radius"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_unsphericity_node(BaseNode &node)
{
  Logger::log()->error("Unsphericity node is deprecated, use Curvatures node");

  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    // zero radius accepted
    int ir = std::max(0, (int)(node.get_attr<FloatAttribute>("radius") * p_out->shape.x));

    hmap::for_each_tile(
        {p_out, p_in},
        [&ir](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out = hmap::gpu::curvature_quadric(*pa_in,
                                                 ir,
                                                 hmap::CurvatureType::CT_UNSPHERICITY);
        },
        node.cfg().cm_gpu);

    p_out->smooth_overlap_buffers();

    // post-process
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
