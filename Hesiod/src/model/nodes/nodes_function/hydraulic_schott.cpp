/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"
#include "highmap/opencl/gpu_opencl.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_hydraulic_schott_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "flow_map", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("duration", "Duration", 0.1f, 0.05f, 2.f);
  node.add_attr<FloatAttribute>("c_erosion", "c_erosion", 1.f, 0.f, 5.f);
  node.add_attr<FloatAttribute>("flow_acc_exponent",
                                "flow_acc_exponent",
                                0.5f,
                                0.01f,
                                2.f);
  node.add_attr<FloatAttribute>("flow_routing_exponent",
                                "flow_routing_exponent",
                                0.8f,
                                0.01f,
                                2.f);

  // attribute(s) order
  node.set_attr_ordered_key(
      {"duration", "c_erosion", "flow_acc_exponent", "flow_routing_exponent"});
}

void compute_hydraulic_schott_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");
    hmap::VirtualArray *p_flow_map = node.get_value_ref<hmap::VirtualArray>("flow_map");

    int iterations = int(node.get_attr<FloatAttribute>("duration") * p_out->shape.x);

    hmap::for_each_tile(
        {p_out, p_in, p_flow_map},
        [&node, iterations](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in, pa_flow_map] = unpack<3>(p_arrays);

          *pa_out = *pa_in;

          hmap::gpu::hydraulic_schott_erosion(
              *pa_out,
              iterations,
              node.get_attr<FloatAttribute>("c_erosion"),
              node.get_attr<FloatAttribute>("flow_acc_exponent"),
              node.get_attr<FloatAttribute>("flow_routing_exponent"),
              pa_flow_map);
        },
        node.cfg().cm_gpu);

    p_out->smooth_overlap_buffers();
  }
}

} // namespace hesiod
