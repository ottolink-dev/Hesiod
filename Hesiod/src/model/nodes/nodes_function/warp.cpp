/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/transform.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_INPUT  = "input";
constexpr const char *P_DX     = "dx";
constexpr const char *P_DY     = "dy";
constexpr const char *P_OUTPUT = "output";

constexpr const char *A_SCALING_X = "scaling.x";
constexpr const char *A_SCALING_Y = "scaling.y";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_warp_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_INPUT);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUTPUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_SCALING_X, "scaling.x", 1.f, -2.f, 2.f);
  add_float(node, A_SCALING_Y, "scaling.y", 1.f, -2.f, 2.f);

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_warp_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto *p_in  = node.get_value_ref<hmap::VirtualArray>(P_INPUT);
  auto *p_dx  = node.get_value_ref<hmap::VirtualArray>(P_DX);
  auto *p_dy  = node.get_value_ref<hmap::VirtualArray>(P_DY);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUTPUT);

  if (!p_in)
    return;

  float sx = node.val<float>(A_SCALING_X);
  float sy = node.val<float>(A_SCALING_Y);

  hmap::for_each_tile(
      {p_out, p_in, p_dx, p_dy},
      [sx, sy](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
      {
        auto [pa_out, pa_in, pa_dx, pa_dy] = unpack<4>(p_arrays);

        hmap::Array dx = pa_dx ? sx * (*pa_dx) : hmap::Array(pa_out->shape);
        hmap::Array dy = pa_dy ? sy * (*pa_dy) : hmap::Array(pa_out->shape);

        *pa_out = *pa_in;

        hmap::gpu::warp(*pa_out, &dx, &dy);
      },
      node.cfg().cm_gpu);

  // post-process
  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
