/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/morphology.hpp"
#include "highmap/opencl/gpu_opencl.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "output";

constexpr const char *A_GPU    = "GPU";
constexpr const char *A_RADIUS = "radius";

void setup_erosion_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_RADIUS, "radius", 0.01f, 0.f, 0.05f);
  add_bool(node, A_GPU, "GPU", HSD_DEFAULT_GPU_MODE);
}

void compute_erosion_node(BaseNode &node)
{
  Logger::log()->error("Erosion node is deprecated, use MorphologyOperators node");

  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    int ir = std::max(1, (int)(node.val<float>(A_RADIUS) * p_out->shape.x));

    if (node.val<bool>(A_GPU))
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
