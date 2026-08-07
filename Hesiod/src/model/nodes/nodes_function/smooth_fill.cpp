/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/opencl/gpu_opencl.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN         = "input";
constexpr const char *P_MASK       = "mask";
constexpr const char *P_OUT        = "output";
constexpr const char *P_DEPOSITION = "deposition";

constexpr const char *A_RADIUS     = "radius";
constexpr const char *A_K          = "k";
constexpr const char *A_NORMALIZED = "normalized_map";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_smooth_fill_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_DEPOSITION, CONFIG(node));

  // --- Attributes

  // clang-format off
  node.add_attr<FloatAttribute>(A_RADIUS, "Radius", 0.05f, 0.001f, 0.2f);
  node.add_attr<FloatAttribute>(A_K, "k", 0.01f, 0.01f, 1.f);
  node.add_attr<BoolAttribute>(A_NORMALIZED, "normalized_map", true);
  // clang-format on

  // --- Attribute(s) order

  node.set_attr_ordered_key({A_RADIUS, A_K, A_NORMALIZED});

  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_smooth_fill_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in         = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_mask       = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  auto *p_out        = node.get_value_ref<hmap::VirtualArray>(P_OUT);
  auto *p_deposition = node.get_value_ref<hmap::VirtualArray>(P_DEPOSITION);

  if (!p_in)
    return;

  // --- Prepare mask

  std::shared_ptr<hmap::VirtualArray> sp_mask = pre_process_mask(node, p_mask, *p_in);

  // --- Params

  // clang-format off
  const auto radius     = node.get_attr<FloatAttribute>(A_RADIUS);
  const auto k          = node.get_attr<FloatAttribute>(A_K);
  const auto normalized = node.get_attr<BoolAttribute>(A_NORMALIZED);
  // clang-format on

  const int ir = std::max(1, int(radius * p_out->shape.x));

  // --- Compute

  hmap::for_each_tile(
      {p_in, p_mask},
      {p_out, p_deposition},
      [&](std::vector<const hmap::Array *> in,
          std::vector<hmap::Array *>       out,
          const hmap::TileRegion &)
      {
        auto [pa_in, pa_mask]        = unpack<2>(in);
        auto [pa_out, pa_deposition] = unpack<2>(out);

        *pa_out = *pa_in;

        hmap::gpu::smooth_fill(*pa_out, ir, pa_mask, k, pa_deposition);
      },
      node.cfg().cm_gpu);

  // --- Post-process

  p_deposition->smooth_overlap_buffers();
  if (normalized)
  {
    p_deposition->remap(0.f, 1.f, node.cfg().cm_cpu);
  }

  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
