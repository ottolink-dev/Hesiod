/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"
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
constexpr const char *P_FLOW_MAP = "flow_map";
constexpr const char *P_IN       = "input";
constexpr const char *P_OUT      = "output";

constexpr const char *A_C_EROSION             = "c_erosion";
constexpr const char *A_DURATION              = "duration";
constexpr const char *A_FLOW_ACC_EXPONENT     = "flow_acc_exponent";
constexpr const char *A_FLOW_ROUTING_EXPONENT = "flow_routing_exponent";

void setup_hydraulic_schott_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_FLOW_MAP, CONFIG(node));

  // attribute(s)
  add_float(node, A_DURATION, "Duration", 0.1f, 0.05f, 2.f);
  add_float(node, A_C_EROSION, "c_erosion", 1.f, 0.f, 5.f);
  add_float(node, A_FLOW_ACC_EXPONENT, "flow_acc_exponent", 0.5f, 0.01f, 2.f);
  add_float(node, A_FLOW_ROUTING_EXPONENT, "flow_routing_exponent", 0.8f, 0.01f, 2.f);
}

void compute_hydraulic_schott_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_out      = node.get_value_ref<hmap::VirtualArray>(P_OUT);
    hmap::VirtualArray *p_flow_map = node.get_value_ref<hmap::VirtualArray>(P_FLOW_MAP);

    int iterations = int(node.val<float>(A_DURATION) * p_out->shape.x);

    hmap::for_each_tile(
        {p_out, p_in, p_flow_map},
        [&node, iterations](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in, pa_flow_map] = unpack<3>(p_arrays);

          *pa_out = *pa_in;

          hmap::gpu::hydraulic_schott_erosion(*pa_out,
                                              iterations,
                                              node.val<float>(A_C_EROSION),
                                              node.val<float>(A_FLOW_ACC_EXPONENT),
                                              node.val<float>(A_FLOW_ROUTING_EXPONENT),
                                              pa_flow_map);
        },
        node.cfg().cm_gpu);

    p_out->smooth_overlap_buffers();
  }
}

} // namespace hesiod
