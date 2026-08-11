/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/primitives.hpp"

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

constexpr const char *P_IN   = "input";
constexpr const char *P_DX   = "dx";
constexpr const char *P_DY   = "dy";
constexpr const char *P_MASK = "mask";
constexpr const char *P_OUT  = "output";

constexpr const char *A_SLOPE             = "slope";
constexpr const char *A_SKEW              = "skew";
constexpr const char *A_DIRECTION_COUNT   = "direction_count";
constexpr const char *A_DIRECTION_OFFSET  = "direction_offset";
constexpr const char *A_RANDOM_DIRECTIONS = "random_directions";
constexpr const char *A_SEED              = "seed";
constexpr const char *A_MIX_RATIO         = "mix_ratio";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_strata_plates_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  // clang-format off
  add_float(node, A_SLOPE, "Slope", 1.f, 0.f, FLT_MAX);
  add_float(node, A_SKEW, "Skew", 0.5f, 0.f, 8.f);
  add_int(node, A_DIRECTION_COUNT, "Direction Count", 4, 1, 8);
  add_int(node, A_DIRECTION_OFFSET, "Direction Offset", 0, 0, 7);
  add_bool(node, A_RANDOM_DIRECTIONS, "Random Directions", false);
  add_seed(node, A_SEED, "Seed");
  add_float(node, A_MIX_RATIO, "Mix Ratio", 1.f, 0.f, 1.f);
  // clang-format on

  // --- Attribute(s) order

  setup_default_noise(node, {.noise_amp = 0.07f, .kw = 4.f, .smoothness = 0.5f});
  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_strata_plates_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto *p_in   = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_dx   = node.get_value_ref<hmap::VirtualArray>(P_DX);
  auto *p_dy   = node.get_value_ref<hmap::VirtualArray>(P_DY);
  auto *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  auto *p_out  = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in)
    return;

  // --- Params

  // clang-format off
  const auto slope             = node.val<float>(A_SLOPE);
  const auto skew              = node.val<float>(A_SKEW);
  const auto direction_count   = node.val<int>(A_DIRECTION_COUNT);
  const auto direction_offset  = node.val<int>(A_DIRECTION_OFFSET);
  const auto random_directions = node.val<bool>(A_RANDOM_DIRECTIONS);
  const auto seed              = node.val<int>(A_SEED);
  const auto mix_ratio         = node.val<float>(A_MIX_RATIO);
  // clang-format on

  float talus = slope / float(p_in->shape.x);

  // --- Prepare mask

  std::shared_ptr<hmap::VirtualArray> sp_mask = pre_process_mask(node, p_mask, *p_in);

  // --- Resolve talus map

  hmap::VirtualArray talus_map = hmap::VirtualArray(CONFIG(node));
  talus_map.fill(talus, node.cfg().cm_cpu);

  // --- Resolve default noise

  hmap::VirtualArray noise_default_x(CONFIG(node));
  hmap::VirtualArray noise_default_y(CONFIG(node));

  generate_noise(node, p_dx, noise_default_x, 0);
  generate_noise(node, p_dy, noise_default_y, 1);

  // --- Compute

  hmap::for_each_tile(
      {p_in, p_dx, p_dy, p_mask, &talus_map},
      {p_out},
      [&](std::vector<const hmap::Array *> p_arrays_in,
          std::vector<hmap::Array *>       p_arrays_out,
          const hmap::TileRegion &)
      {
        auto [pa_in, pa_dx, pa_dy, pa_mask, pa_talus] = unpack<5>(p_arrays_in);
        auto [pa_out]                                 = unpack<1>(p_arrays_out);

        *pa_out = *pa_in;

        hmap::gpu::strata_plates(*pa_out,
                                 *pa_talus,
                                 direction_offset,
                                 direction_count,
                                 random_directions,
                                 seed,
                                 -FLT_MAX, // vmin
                                 skew,
                                 mix_ratio,
                                 pa_mask,
                                 pa_dx,
                                 pa_dy);
      },
      node.cfg().cm_gpu);

  // --- Post-process

  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
