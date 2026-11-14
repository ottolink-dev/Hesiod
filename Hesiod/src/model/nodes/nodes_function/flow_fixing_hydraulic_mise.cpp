/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion/hydraulic_erosion.hpp"
#include "highmap/hydrology/hydrology.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "output";

// Hydraulic MISE
constexpr const char *A_SEED      = "seed";
constexpr const char *A_STRENGTH  = "strength";
constexpr const char *A_BASE_RES  = "base_res";
constexpr const char *A_LAKE_FILL = "lake_fill";

// Flow Fixing
constexpr const char *A_FLOW_THRESHOLD         = "flow_threshold";
constexpr const char *A_MERGING_RADIUS         = "merging_radius";
constexpr const char *A_PREFILTER_IR           = "prefilter_ir";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_flow_fixing_hydraulic_mise_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  // clang-format off
  node.set_current_category("Hydraulic MISE");
  add_seed(node, A_SEED, "Seed");
  add_float(node, A_STRENGTH, "Erosion Strength", 0.25f, 0.f, 2.f);
  add_int(node, A_BASE_RES, "Base Resolution", 64, 16, 256);
  add_float(node, A_LAKE_FILL, "Lake Fill Ratio", 0.f, 0.f, 1.f);

  node.set_current_category("Flow Fixing");
  add_float(node, A_FLOW_THRESHOLD, "Flow Threshold", 1e-3f, 1e-5f, 1e-1f, "{:.2e}", true);
  add_float(node, A_MERGING_RADIUS, "Merging Radius", 0.01f, 0.f, 0.1f, "{:.3f}");
  add_int(node, A_PREFILTER_IR, "Prefilter Radius", 8, 0, 32);
  // clang-format on

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_flow_fixing_hydraulic_mise_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  const auto *p_in  = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto       *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in || !p_out)
    return;

  // --- Parameters

  hmap::MiseParams params;
  params.seed      = static_cast<uint>(node.val<int>(A_SEED));
  params.strength  = node.val<float>(A_STRENGTH);
  params.base_res  = node.val<int>(A_BASE_RES);
  params.lake_fill = node.val<float>(A_LAKE_FILL);

  // clang-format off
  const auto flow_threshold         = node.val<float>(A_FLOW_THRESHOLD);
  const auto merging_distance       = node.val_pixel_radius(A_MERGING_RADIUS, 1);
  const auto prefilter_ir           = node.val<int>(A_PREFILTER_IR);
  // clang-format on

  // --- Compute

  hmap::for_each_tile(
      {p_in},
      {p_out},
      [&](std::vector<const hmap::Array *> p_arrays_in,
          std::vector<hmap::Array *>       p_arrays_out,
          const hmap::TileRegion &)
      {
        auto [pa_in]  = unpack<1>(p_arrays_in);
        auto [pa_out] = unpack<1>(p_arrays_out);

        *pa_out = hmap::flow_fixing_hydraulic_mise(*pa_in,
                                                   params,
                                                   flow_threshold,
                                                   merging_distance,
                                                   prefilter_ir);
      },
      node.cfg().cm_single_array);

  // --- Post-process

  p_out->sync_overlap_buffers();
  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
