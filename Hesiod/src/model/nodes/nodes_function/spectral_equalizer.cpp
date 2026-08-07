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

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_INPUT  = "input";
constexpr const char *P_MASK   = "mask";
constexpr const char *P_OUTPUT = "output";

constexpr const char *A_EQ   = "eq";
constexpr const char *A_RMIN = "rmin";
constexpr const char *A_RMAX = "rmax";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_spectral_equalizer_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_INPUT);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUTPUT, CONFIG(node));

  // --- Attributes

  std::vector<float> weights(6, 1.f);

  // clang-format off
  add_curve(node, A_EQ, "Band Weights", weights, 0.f, 2.f);
  add_float(node, A_RMIN, "Radius Min.", 0.05f, 0.f, 0.5f);
  add_float(node, A_RMAX, "Radius Max.", 0.25f, 0.f, 0.5f);
  // clang-format on

  // --- Attribute(s) order

  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_spectral_equalizer_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in   = node.get_value_ref<hmap::VirtualArray>(P_INPUT);
  auto *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  auto *p_out  = node.get_value_ref<hmap::VirtualArray>(P_OUTPUT);

  if (!p_in)
    return;

  // --- Params

  // clang-format off
  const auto weights = node.val<std::vector<float>>(A_EQ);
  const auto rmin    = node.val<float>(A_RMIN);
  const auto rmax    = node.val<float>(A_RMAX);
  // clang-format on

  int ir_min = std::max(1, (int)(rmin * p_out->shape.x));
  int ir_max = std::max(1, (int)(rmax * p_out->shape.x));

  // --- Prepare mask

  std::shared_ptr<hmap::VirtualArray> sp_mask = pre_process_mask(node, p_mask, *p_in);

  // --- Compute

  hmap::for_each_tile(
      {p_in, p_mask},
      {p_out},
      [&](std::vector<const hmap::Array *> p_arrays_in,
          std::vector<hmap::Array *>       p_arrays_out,
          const hmap::TileRegion &)
      {
        const auto [pa_in, pa_mask] = unpack<2>(p_arrays_in);
        auto [pa_out]               = unpack<1>(p_arrays_out);

        *pa_out = hmap::gpu::spectral_equalizer(*pa_in, weights, ir_min, ir_max, pa_mask);
      },
      node.cfg().cm_gpu);

  // --- Post-process

  p_out->smooth_overlap_buffers();
  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
