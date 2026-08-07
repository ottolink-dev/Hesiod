/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/selector.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
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

constexpr const char *A_CLIPPING_RATIO  = "clipping_ratio";
constexpr const char *A_FLOW_GAMMA      = "flow_gamma";
constexpr const char *A_FLOW_WEIGHT     = "flow_weight";
constexpr const char *A_GRADIENT_WEIGHT = "gradient_weight";
constexpr const char *A_RADIUS_GRADIENT = "radius_gradient";
constexpr const char *A_TALUS_REF       = "talus_ref";

void setup_select_soil_flow_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_RADIUS_GRADIENT, "Gradient Radius", 0.f, 0.f, 0.1f);
  add_float(node, A_GRADIENT_WEIGHT, "Gradient Weight", 1.f, 0.f, 1.f);
  add_float(node, A_FLOW_WEIGHT, "Flow Weight", 0.01f, 0.f, 1.f);
  add_float(node, A_TALUS_REF, "Ref. Talus", 10.f, 0.01f, 32.f);
  add_float(node, A_CLIPPING_RATIO, "Clipping Ratio", 50.f, 0.1f, 100.f);
  add_float(node, A_FLOW_GAMMA, "Flow Distrib. Exponent", 1.f, 0.01f, 4.f);

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

void compute_select_soil_flow_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    int   nx    = p_out->shape.x; // for gradient scaling
    int   ir    = std::max(1, (int)(node.val<float>(A_RADIUS_GRADIENT) * nx));
    float talus = node.val<float>(A_TALUS_REF) / nx;

    // --- compute mask

    hmap::for_each_tile(
        {p_out, p_in},
        [&node, nx, ir, talus](std::vector<hmap::Array *> p_arrays,
                               const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          float k_smooth       = 0.01f; // little influence

          *pa_out = hmap::gpu::select_soil_flow(*pa_in,
                                                ir,
                                                node.val<float>(A_GRADIENT_WEIGHT),
                                                (float)nx,
                                                node.val<float>(A_FLOW_WEIGHT),
                                                talus,
                                                node.val<float>(A_CLIPPING_RATIO),
                                                node.val<float>(A_FLOW_GAMMA),
                                                k_smooth);
        },
        node.cfg().cm_gpu);

    // post-process
    p_out->smooth_overlap_buffers();
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
