/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/curvature.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/range.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_accumulation_curvature_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("radius", "radius", 0.02f, 0.f, 0.2f);
  node.add_attr<BoolAttribute>("clamp_max", "clamp_max", false);
  node.add_attr<FloatAttribute>("vc_max", "vc_max", 0.05f, 0.f, 0.2f);

  // attribute(s) order
  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Main Parameters",
                             "radius",
                             "clamp_max",
                             "vc_max",
                             "_GROUPBOX_END_"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

void compute_accumulation_curvature_node(BaseNode &node)
{
  Logger::log()->error("AccumulationCurvature node is deprecated, use Curvatures node");

  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    int ir = std::max(1, (int)(node.get_attr<FloatAttribute>("radius") * p_out->shape.x));
    int nx = p_out->shape.x; // for gradient scaling

    hmap::for_each_tile(
        {p_out, p_in},
        [&node, ir, nx](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          // nx^2 is gradient scaling...
          *pa_out = nx * nx *
                    hmap::gpu::curvature_quadric(*pa_in,
                                                 ir,
                                                 hmap::CurvatureType::CT_ACCUMULATION);

          // truncate high values if requested
          if (node.get_attr<BoolAttribute>("clamp_max"))
            hmap::clamp_max(*pa_out, node.get_attr<FloatAttribute>("vc_max"));
        },
        node.cfg().cm_gpu);

    p_out->smooth_overlap_buffers();

    // post-process
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
