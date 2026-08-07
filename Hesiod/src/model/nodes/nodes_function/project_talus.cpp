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

constexpr const char *P_IN   = "input";
constexpr const char *P_MASK = "mask";
constexpr const char *P_OUT  = "output";

constexpr const char *A_SLOPE     = "slope";
constexpr const char *A_DIRECTION = "direction";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_project_talus_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  node.add_attr<FloatAttribute>(A_SLOPE, "Slope", 1.f, 0.f, FLT_MAX);
  node.add_attr<IntAttribute>(A_DIRECTION, "Propagation Direction (D8)", 0, 0, 7);

  // --- Attribute(s) order

  node.set_attr_ordered_key(
      {"_GROUPBOX_BEGIN_Main Parameters", A_SLOPE, A_DIRECTION, "_GROUPBOX_END_"});

  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_project_talus_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in   = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  auto *p_out  = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in)
    return;

  // --- Params

  // clang-format off
  const auto slope     = node.get_attr<FloatAttribute>(A_SLOPE);
  const auto direction = node.get_attr<IntAttribute>(A_DIRECTION);
  // clang-format on

  float talus = slope / float(p_in->shape.x);

  // --- Prepare mask

  std::shared_ptr<hmap::VirtualArray> sp_mask = pre_process_mask(node, p_mask, *p_in);

  // --- Compute

  hmap::for_each_tile(
      {p_in, p_mask},
      {p_out},
      [&](std::vector<const hmap::Array *> in,
          std::vector<hmap::Array *>       out,
          const hmap::TileRegion          &region)
      {
        auto [pa_in, pa_mask] = unpack<2>(in);
        auto [pa_out]         = unpack<1>(out);

        *pa_out = hmap::gpu::project_talus_along_direction(
            *pa_in,
            hmap::Array(region.shape, talus),
            pa_mask,
            direction);
      },
      node.cfg().cm_gpu);

  // --- Post-process

  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
