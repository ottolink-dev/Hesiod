/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/hydrology/hydrology.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_ELEVATION   = "elevation";
constexpr const char *P_WATER_DEPTH = "water_depth";
constexpr const char *P_WATER_MASK  = "water_mask";

constexpr const char *A_ITERATIONS     = "iterations";
constexpr const char *A_MASK_THRESHOLD = "mask_threshold";
constexpr const char *A_TOLERANCE      = "tolerance";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_water_depth_from_mask_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ELEVATION);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_WATER_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_WATER_DEPTH, CONFIG(node));

  // --- Attributes

  // clang-format off
  add_float(node, A_MASK_THRESHOLD, "Mask Activation Threshold", 0.01f, 0.f, 1.f, "{:.3f}");
  add_float(node, A_TOLERANCE, "Convergence Tolerance", 1e-6f, 1e-8f, 1e-4f, "{:.3e}", true);
  add_int(node, A_ITERATIONS, "Max Iterations", 500, 1, INT_MAX);
  // clang-format on
}

void compute_water_depth_from_mask_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_z     = node.get_value_ref<hmap::VirtualArray>(P_ELEVATION);
  auto *p_mask  = node.get_value_ref<hmap::VirtualArray>(P_WATER_MASK);
  auto *p_depth = node.get_value_ref<hmap::VirtualArray>(P_WATER_DEPTH);

  if (!p_z || !p_mask)
    return;

  // --- Compute

  hmap::for_each_tile(
      {p_z, p_mask},
      {p_depth},
      [&node](std::vector<const hmap::Array *> p_arrays_in,
              std::vector<hmap::Array *>       p_arrays_out,
              const hmap::TileRegion &)
      {
        auto [pa_z, pa_mask] = unpack<2>(p_arrays_in);
        auto [pa_depth]      = unpack<1>(p_arrays_out);

        *pa_depth = hmap::gpu::water_depth_from_mask(*pa_z,
                                                     *pa_mask,
                                                     node.val<float>(A_MASK_THRESHOLD),
                                                     node.val<int>(A_ITERATIONS),
                                                     node.val<float>(A_TOLERANCE));
      },
      node.cfg().cm_gpu);

  // --- Post-process

  p_depth->sync_overlap_buffers();
}

} // namespace hesiod
