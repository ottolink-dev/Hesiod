/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/morphology.hpp"
#include "highmap/opencl/gpu_opencl.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_erosion_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("radius", "radius", 0.01f, 0.f, 0.05f);
  node.add_attr<BoolAttribute>("GPU", "GPU", HSD_DEFAULT_GPU_MODE);

  // attribute(s) order
  node.set_attr_ordered_key({"radius", "_SEPARATOR_", "GPU"});
}

void compute_erosion_node(BaseNode &node)
{
  Logger::log()->error("Erosion node is deprecated, use MorphologyOperators node");

  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    int ir = std::max(1, (int)(node.get_attr<FloatAttribute>("radius") * p_out->shape.x));

    if (node.get_attr<BoolAttribute>("GPU"))
    {
      hmap::for_each_tile(
          {p_out, p_in},
          [&ir](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
          {
            hmap::Array *pa_out = p_arrays[0];
            hmap::Array *pa_in  = p_arrays[1];

            *pa_out = hmap::gpu::erosion(*pa_in, ir);
          },
          node.cfg().cm_gpu);
    }
    else
    {
      hmap::for_each_tile(
          {p_out, p_in},
          [&ir](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
          {
            hmap::Array *pa_out = p_arrays[0];
            hmap::Array *pa_in  = p_arrays[1];

            *pa_out = hmap::erosion(*pa_in, ir);
          },
          node.cfg().cm_cpu);
    }

    p_out->smooth_overlap_buffers();
  }
}

} // namespace hesiod
