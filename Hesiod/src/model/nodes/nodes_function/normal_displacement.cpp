/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
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
constexpr const char *P_IN   = "input";
constexpr const char *P_MASK = "mask";
constexpr const char *P_OUT  = "output";

constexpr const char *A_AMOUNT     = "amount";
constexpr const char *A_ITERATIONS = "iterations";
constexpr const char *A_RADIUS     = "radius";
constexpr const char *A_REVERSE    = "reverse";

void setup_normal_displacement_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_RADIUS, "radius", 0.05f, 0.f, 0.2f);
  add_float(node, A_AMOUNT, "amount", 5.f, 0.f, 20.f);
  add_bool(node, A_REVERSE, "reverse", false);
  add_int(node, A_ITERATIONS, "iterations", 3, 1, 10);
}

void compute_normal_displacement_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);
    hmap::VirtualArray *p_out  = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    int ir = std::max(0, (int)(node.val<float>(A_RADIUS) * p_in->shape.x));

    for (int it = 0; it < node.val<int>(A_ITERATIONS); it++)
    {
      hmap::for_each_tile(
          {p_out, p_in, p_mask},
          [&node, &ir](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
          {
            auto [pa_out, pa_in, pa_mask] = unpack<3>(p_arrays);

            *pa_out = *pa_in;

            hmap::gpu::normal_displacement(*pa_out,
                                           pa_mask,
                                           node.val<float>(A_AMOUNT),
                                           ir,
                                           node.val<bool>(A_REVERSE));
          },
          node.cfg().cm_gpu);

      p_out->smooth_overlap_buffers();
    }
  }
}

} // namespace hesiod
