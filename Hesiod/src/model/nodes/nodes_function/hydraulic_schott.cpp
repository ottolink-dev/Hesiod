/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"
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

constexpr const char *P_ELEVATION = "elevation";
constexpr const char *P_TALUS_MAP = "talus_map";
constexpr const char *P_MASK      = "mask";
constexpr const char *P_OUT       = "output";
constexpr const char *P_FLOW_MAP  = "flow_map";

constexpr const char *A_DURATION                  = "duration";
constexpr const char *A_TALUS                     = "talus_global";
constexpr const char *A_SCALE_TALUS               = "scale_talus_with_elevation";
constexpr const char *A_C_EROSION                 = "c_erosion";
constexpr const char *A_C_THERMAL                 = "c_thermal";
constexpr const char *A_C_DEPOSITION              = "c_deposition";
constexpr const char *A_FLOW_ACC_EXPONENT         = "flow_acc_exponent";
constexpr const char *A_FLOW_ACC_EXPONENT_DEPO    = "flow_acc_exponent_depo";
constexpr const char *A_FLOW_ROUTING_EXPONENT     = "flow_routing_exponent";
constexpr const char *A_THERMAL_WEIGHT            = "thermal_weight";
constexpr const char *A_DEPOSITION_WEIGHT         = "deposition_weight";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_hydraulic_schott_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // ports
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ELEVATION);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_TALUS_MAP);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_FLOW_MAP, CONFIG(node));

  // attributes
  // clang-format off
  node.set_current_category("Simulation");
  add_float(node, A_DURATION, "Duration", 0.5f, 0.05f, 2.f);

  node.set_current_category("Erosion & Deposition");
  add_float(node, A_C_EROSION, "Erosion Rate", 1.f, 0.f, 5.f);
  add_float(node, A_C_THERMAL, "Thermal Rate", 0.1f, 0.f, 1.f);
  add_float(node, A_C_DEPOSITION, "Deposition Rate", 0.2f, 0.f, 1.f);
  add_float(node, A_THERMAL_WEIGHT, "Thermal Weight", 1.f, 0.f, 2.f);
  add_float(node, A_DEPOSITION_WEIGHT, "Deposition Weight", 1.f, 0.f, 2.f);

  node.set_current_category("Flow Routing");
  add_float(node, A_FLOW_ACC_EXPONENT, "Flow Acc. Exponent (Erosion)", 0.8f, 0.01f, 2.f);
  add_float(node, A_FLOW_ACC_EXPONENT_DEPO, "Flow Acc. Exponent (Deposition)", 0.5f, 0.01f, 2.f);
  add_float(node, A_FLOW_ROUTING_EXPONENT, "Flow Routing Exponent", 1.3f, 0.01f, 3.f);

  node.set_current_category("Talus Slope");
  add_float(node, A_TALUS, "Talus Slope", 2.f, 0.f, 10.f);
  add_bool(node, A_SCALE_TALUS, "Scale Talus with Elevation", false);
  // clang-format on

  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_hydraulic_schott_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_z         = node.get_value_ref<hmap::VirtualArray>(P_ELEVATION);
  auto *p_talus_map = node.get_value_ref<hmap::VirtualArray>(P_TALUS_MAP);
  auto *p_mask      = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  auto *p_out       = node.get_value_ref<hmap::VirtualArray>(P_OUT);
  auto *p_flow_map  = node.get_value_ref<hmap::VirtualArray>(P_FLOW_MAP);

  if (!p_z)
    return;

  // --- Prepare mask

  hmap::VirtualArray mask_default = pre_process_mask(node, p_mask, *p_z);
  if (!mask_default.empty())
    p_mask = &mask_default;

  // --- Params

  // clang-format off
  const auto duration               = node.val<float>(A_DURATION);
  const auto c_erosion              = node.val<float>(A_C_EROSION);
  const auto c_thermal              = node.val<float>(A_C_THERMAL);
  const auto c_deposition           = node.val<float>(A_C_DEPOSITION);
  const auto flow_acc_exponent      = node.val<float>(A_FLOW_ACC_EXPONENT);
  const auto flow_acc_exponent_depo = node.val<float>(A_FLOW_ACC_EXPONENT_DEPO);
  const auto flow_routing_exponent  = node.val<float>(A_FLOW_ROUTING_EXPONENT);
  const auto thermal_weight         = node.val<float>(A_THERMAL_WEIGHT);
  const auto deposition_weight      = node.val<float>(A_DEPOSITION_WEIGHT);
  const auto talus_global           = node.val<float>(A_TALUS);
  const auto scale_talus            = node.val<bool>(A_SCALE_TALUS);
  // clang-format on

  const int   iterations = int(duration * p_out->shape.x);
  const float talus      = talus_global / float(p_out->shape.x);

  // --- Talus map

  hmap::VirtualArray talus_default(CONFIG(node));

  if (!p_talus_map)
  {
    talus_default.fill(talus, node.cfg().cm_cpu);

    if (scale_talus)
    {
      talus_default.copy_from(*p_z, node.cfg().cm_cpu);
      talus_default.remap(talus / 100.f, talus, node.cfg().cm_cpu);
    }

    p_talus_map = &talus_default;
  }

  // --- Compute

  hmap::for_each_tile(
      {p_z, p_talus_map, p_mask},
      {p_out, p_flow_map},
      [&](std::vector<const hmap::Array *> p_arrays_in,
          std::vector<hmap::Array *>       p_arrays_out,
          const hmap::TileRegion &)
      {
        auto [pa_z, pa_talus, pa_mask] = unpack<3>(p_arrays_in);
        auto [pa_out, pa_flow_map]     = unpack<2>(p_arrays_out);

        *pa_out = *pa_z;

        hmap::gpu::hydraulic_schott(*pa_out,
                                    pa_mask,
                                    iterations,
                                    *pa_talus,
                                    c_erosion,
                                    c_thermal,
                                    c_deposition,
                                    flow_acc_exponent,
                                    flow_acc_exponent_depo,
                                    flow_routing_exponent,
                                    thermal_weight,
                                    deposition_weight,
                                    pa_flow_map);
      },
      node.cfg().cm_gpu);

  // --- Post-treatments

  p_out->sync_overlap_buffers();

  if (p_flow_map)
  {
    p_flow_map->sync_overlap_buffers();
    p_flow_map->remap(0.f, 1.f, node.cfg().cm_cpu);
  }

  post_process_heightmap(node, *p_out, p_z);
}

} // namespace hesiod

