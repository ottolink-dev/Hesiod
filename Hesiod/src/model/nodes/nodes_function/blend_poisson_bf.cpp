/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/blending.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN1  = "input 1";
constexpr const char *P_IN2  = "input 2";
constexpr const char *P_MASK = "mask";
constexpr const char *P_OUT  = "output";

constexpr const char *A_ITERATIONS = "iterations";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_blend_poisson_bf_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN1);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN2);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  add_int(node, A_ITERATIONS, "iterations", 500, 1, INT_MAX);

  setup_post_process_heightmap_attributes(
      node,
      {.add_mix = false, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_blend_poisson_bf_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in1  = node.get_value_ref<hmap::VirtualArray>(P_IN1);
  auto *p_in2  = node.get_value_ref<hmap::VirtualArray>(P_IN2);
  auto *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  auto *p_out  = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in1 || !p_in2 || !p_out)
    return;

  // --- Params

  const auto iterations = node.val<int>(A_ITERATIONS);

  // --- Compute

  hmap::for_each_tile(
      {p_in1, p_in2, p_mask},
      {p_out},
      [iterations](std::vector<const hmap::Array *> p_arrays_in,
                   std::vector<hmap::Array *>       p_arrays_out,
                   const hmap::TileRegion &)
      {
        auto [pa_in1, pa_in2, pa_mask] = unpack<3>(p_arrays_in);
        auto [pa_out]                  = unpack<1>(p_arrays_out);

        *pa_out = hmap::gpu::blend_poisson_bf(*pa_in1, *pa_in2, iterations, pa_mask);
      },
      node.cfg().cm_gpu);

  p_out->sync_overlap_buffers();

  // --- Post-process

  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
