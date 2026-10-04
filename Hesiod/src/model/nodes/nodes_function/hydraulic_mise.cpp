/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_INPUT       = "input";
constexpr const char *P_BEDROCK     = "bedrock";
constexpr const char *P_ERODIBILITY = "erodibility";
constexpr const char *P_MOISTURE    = "moisture";
constexpr const char *P_OUTLET      = "outlet";
constexpr const char *P_MASK        = "mask";

constexpr const char *P_OUTPUT     = "output";
constexpr const char *P_SEDIMENT   = "sediment";
constexpr const char *P_FLOW       = "flow";
constexpr const char *P_EROSION    = "erosion";
constexpr const char *P_DEPOSITION = "deposition";

// Fluvial incision
constexpr const char *A_SEED        = "seed";
constexpr const char *A_STRENGTH    = "strength";
constexpr const char *A_AREA_EXP    = "area_exp";
constexpr const char *A_DOWNCUTTING = "downcutting";

// Hillslopes
constexpr const char *A_THERMAL = "thermal";
constexpr const char *A_DEBRIS  = "debris";
constexpr const char *A_TALUS   = "talus";
constexpr const char *A_CLIFF   = "cliff";

// Sediment & deposition
constexpr const char *A_DEPOSITION_RATE      = "deposition_rate";
constexpr const char *A_DEPOSITION_SLOPE     = "deposition_slope";
constexpr const char *A_DEPOSITION_EXP       = "deposition_exp";
constexpr const char *A_DEPOSITION_AREA      = "deposition_area";
constexpr const char *A_LAKE_FILL            = "lake_fill";
constexpr const char *A_SEDIMENT_ERODIBILITY = "sediment_erodibility";
constexpr const char *A_SEDIMENT_TALUS       = "sediment_talus";
constexpr const char *A_SEDIMENT_RELAX_ITERS = "sediment_relax_iters";

// Flow routing
constexpr const char *A_RECEIVER_EXP         = "receiver_exp";
constexpr const char *A_SLOPE_CORRECTION_MAX = "slope_correction_max";

// Multiscale schedule
constexpr const char *A_BASE_RES    = "base_res";
constexpr const char *A_SHARE_RATIO = "share_ratio";
constexpr const char *A_ITERS0      = "iters0";
constexpr const char *A_ITERS_DECAY = "iters_decay";
constexpr const char *A_ITERS_MIN   = "iters_min";
constexpr const char *A_SKIP_FINEST = "skip_finest";
constexpr const char *A_WARP        = "warp";
constexpr const char *A_ANTIALIAS   = "antialias";

// Solver options
constexpr const char *A_EXTRAPOLATE_BORDER = "extrapolate_border";
constexpr const char *A_EXACT_FLOOD        = "exact_flood";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_hydraulic_mise_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_INPUT);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_BEDROCK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ERODIBILITY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MOISTURE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_OUTLET);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);

  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUTPUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_SEDIMENT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_FLOW, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_EROSION, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_DEPOSITION, CONFIG(node));

  // attribute(s)
  // clang-format off
  node.set_current_category("Fluvial Incision");
  add_seed(node, A_SEED, "Seed");
  add_float(node, A_STRENGTH, "Erosion Strength", 0.25f, 0.f, 2.f);
  add_float(node, A_AREA_EXP, "Drainage Area Exponent", 0.45f, 0.05f, 0.8f);
  add_float(node, A_DOWNCUTTING, "Downcutting Limit", 0.3f, 0.01f, 1.f);

  node.set_current_category("Hillslopes");
  add_float(node, A_THERMAL, "Thermal Rate", 0.05f, 0.f, 1.f);
  add_float(node, A_DEBRIS, "Debris Rate", 200.f, 0.f, 1000.f);
  add_float(node, A_TALUS, "Talus Slope", 2.f, 0.1f, 10.f);
  add_float(node, A_CLIFF, "Cliff Slope Limit", 6.8f, 0.f, 20.f);

  node.set_current_category("Sediment & Deposition");
  add_float(node, A_DEPOSITION_RATE, "Deposition Rate", 1.f, 0.f, 2.f);
  add_float(node, A_DEPOSITION_SLOPE, "Deposition Slope", 1.8f, 0.f, 10.f);
  add_float(node, A_DEPOSITION_EXP, "Deposition Exponent", 0.1f, 0.f, 1.f);
  add_float(node, A_DEPOSITION_AREA, "Deposition Area", 1e-4f, 1e-6f, 1e-1f, "{:.2e}", true);
  add_float(node, A_LAKE_FILL, "Lake Fill Ratio", 1.f, 0.f, 1.f);
  add_float(node, A_SEDIMENT_ERODIBILITY, "Sediment Erodibility", 2.f, 0.1f, 10.f);
  add_float(node, A_SEDIMENT_TALUS, "Sediment Talus Slope", 1.4f, 0.1f, 10.f);
  add_int(node, A_SEDIMENT_RELAX_ITERS, "Sediment Relax Iterations", 2, 0, 10);

  node.set_current_category("Flow Routing");
  add_float(node, A_RECEIVER_EXP, "Receiver Exponent", 2.f, 0.1f, 3.f);
  add_float(node, A_SLOPE_CORRECTION_MAX, "Max Slope Correction", 2.f, 1.f, 10.f);

  node.set_current_category("Multiscale Schedule");
  add_int(node, A_BASE_RES, "Base Resolution", 128, 16, 1024);
  add_float(node, A_SHARE_RATIO, "Strength Share Ratio", 0.6f, 0.1f, 1.f);
  add_int(node, A_ITERS0, "Base Level Iterations", 1, 1, 50);
  add_float(node, A_ITERS_DECAY, "Iteration Decay", 0.75f, 0.1f, 1.f);
  add_int(node, A_ITERS_MIN, "Min Iterations", 3, 1, 20);
  add_int(node, A_SKIP_FINEST, "Skip Finest Levels", 0, 0, 4);
  add_float(node, A_WARP, "Domain Warp", 0.2f, 0.f, 2.f);
  add_float(node, A_ANTIALIAS, "Antialias Factor", 0.1f, 0.f, 1.f);

  node.set_current_category("Solver Options");
  add_bool(node, A_EXTRAPOLATE_BORDER, "Extrapolate Border", true);
  add_bool(node, A_EXACT_FLOOD, "Exact Flood Queue", false);
  // clang-format on

  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_hydraulic_mise_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  const auto *p_in          = node.get_value_ref<hmap::VirtualArray>(P_INPUT);
  const auto *p_bedrock     = node.get_value_ref<hmap::VirtualArray>(P_BEDROCK);
  const auto *p_erodibility = node.get_value_ref<hmap::VirtualArray>(P_ERODIBILITY);
  const auto *p_moisture    = node.get_value_ref<hmap::VirtualArray>(P_MOISTURE);
  const auto *p_outlet      = node.get_value_ref<hmap::VirtualArray>(P_OUTLET);
  const auto *p_mask        = node.get_value_ref<hmap::VirtualArray>(P_MASK);

  auto *p_out        = node.get_value_ref<hmap::VirtualArray>(P_OUTPUT);
  auto *p_sediment   = node.get_value_ref<hmap::VirtualArray>(P_SEDIMENT);
  auto *p_flow       = node.get_value_ref<hmap::VirtualArray>(P_FLOW);
  auto *p_erosion    = node.get_value_ref<hmap::VirtualArray>(P_EROSION);
  auto *p_deposition = node.get_value_ref<hmap::VirtualArray>(P_DEPOSITION);

  if (!p_in)
    return;

  // --- Parameters

  hmap::MiseParams params;
  params.seed        = static_cast<std::uint32_t>(node.val<int>(A_SEED));
  params.strength    = node.val<float>(A_STRENGTH);
  params.area_exp    = node.val<float>(A_AREA_EXP);
  params.downcutting = node.val<float>(A_DOWNCUTTING);

  params.thermal = node.val<float>(A_THERMAL);
  params.debris  = node.val<float>(A_DEBRIS);
  params.talus   = node.val<float>(A_TALUS);
  params.cliff   = node.val<float>(A_CLIFF);

  params.deposition_rate      = node.val<float>(A_DEPOSITION_RATE);
  params.deposition_slope     = node.val<float>(A_DEPOSITION_SLOPE);
  params.deposition_exp       = node.val<float>(A_DEPOSITION_EXP);
  params.deposition_area      = node.val<float>(A_DEPOSITION_AREA);
  params.lake_fill            = node.val<float>(A_LAKE_FILL);
  params.sediment_erodibility = node.val<float>(A_SEDIMENT_ERODIBILITY);
  params.sediment_talus       = node.val<float>(A_SEDIMENT_TALUS);
  params.sediment_relax_iters = node.val<int>(A_SEDIMENT_RELAX_ITERS);

  params.receiver_exp         = node.val<float>(A_RECEIVER_EXP);
  params.slope_correction_max = node.val<float>(A_SLOPE_CORRECTION_MAX);

  params.base_res    = node.val<int>(A_BASE_RES);
  params.share_ratio = node.val<float>(A_SHARE_RATIO);
  params.iters0      = node.val<int>(A_ITERS0);
  params.iters_decay = node.val<float>(A_ITERS_DECAY);
  params.iters_min   = node.val<int>(A_ITERS_MIN);
  params.skip_finest = node.val<int>(A_SKIP_FINEST);
  params.warp        = node.val<float>(A_WARP);
  params.antialias   = node.val<float>(A_ANTIALIAS);

  params.extrapolate_border = node.val<bool>(A_EXTRAPOLATE_BORDER);
  params.exact_flood        = node.val<bool>(A_EXACT_FLOOD);

  // --- Prepare mask

  hmap::VirtualArray mask_default = pre_process_mask(node, p_mask, *p_in);
  if (!mask_default.empty())
    p_mask = &mask_default;

  // --- Flatness check

  float hmin = p_in->min(node.cfg().cm_cpu);
  float hmax = p_in->max(node.cfg().cm_cpu);

  if (hmax - hmin < 1.0e-6f)
  {
    Logger::log()->warn(
        "HydraulicMise [{}]: input is flat/near-constant (value range ~ 0). "
        "Erosion is gradient-driven and will produce little or no change.",
        node.get_id());
    return;
  }

  // --- Compute

  hmap::for_each_tile(
      {p_in, p_mask, p_bedrock, p_erodibility, p_moisture, p_outlet},
      {p_out, p_sediment, p_flow, p_erosion, p_deposition},
      [&params](std::vector<const hmap::Array *> p_arrays_in,
                std::vector<hmap::Array *>       p_arrays_out,
                const hmap::TileRegion &)
      {
        auto [pa_in,
              pa_mask,
              pa_bedrock,
              pa_erodibility,
              pa_moisture,
              pa_outlet] = unpack<6>(p_arrays_in);
        auto [pa_out, pa_sediment, pa_flow, pa_erosion, pa_deposition] = unpack<5>(
            p_arrays_out);

        *pa_out = *pa_in;

        hmap::hydraulic_mise(*pa_out,
                             pa_mask,
                             params,
                             pa_bedrock,
                             pa_erodibility,
                             pa_moisture,
                             pa_outlet,
                             pa_sediment,
                             pa_flow,
                             pa_erosion,
                             pa_deposition);
      },
      node.cfg().cm_cpu);

  // --- Post-treatments

  p_out->sync_overlap_buffers();

  p_sediment->sync_overlap_buffers();
  p_sediment->remap(0.f, 1.f, node.cfg().cm_cpu);

  p_flow->sync_overlap_buffers();
  p_flow->remap(0.f, 1.f, node.cfg().cm_cpu);

  p_erosion->sync_overlap_buffers();
  p_erosion->remap(0.f, 1.f, node.cfg().cm_cpu);

  p_deposition->sync_overlap_buffers();
  p_deposition->remap(0.f, 1.f, node.cfg().cm_cpu);

  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
