/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"
#include "highmap/opencl/gpu_opencl.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_INPUT      = "input";
constexpr const char *P_BEDROCK    = "bedrock";
constexpr const char *P_MOISTURE   = "moisture";
constexpr const char *P_MASK       = "mask";
constexpr const char *P_OUTPUT     = "output";
constexpr const char *P_EROSION    = "erosion";
constexpr const char *P_DEPOSITION = "deposition";
constexpr const char *P_FLOW_MAP   = "flow_map";

constexpr const char *A_SEED                      = "seed";
constexpr const char *A_LEVELS                    = "levels";
constexpr const char *A_MIX                       = "mix";
constexpr const char *A_WARP                      = "warp";
constexpr const char *A_C_EROSION                 = "c_erosion";
constexpr const char *A_TALUS_REF                 = "talus_ref";
constexpr const char *A_SATURATION_RATIO          = "saturation_ratio";
constexpr const char *A_GRADIENT_POWER            = "gradient_power";
constexpr const char *A_GRADIENT_SCALING_RATIO    = "gradient_scaling_ratio";
constexpr const char *A_GRADIENT_PREFILTER_RADIUS = "gradient_prefilter_radius";
constexpr const char *A_DEPOSITION_RADIUS         = "deposition_radius";
constexpr const char *A_DEPOSITION_SCALE_RATIO    = "deposition_scale_ratio";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_hydraulic_stream_log_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_INPUT);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_BEDROCK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MOISTURE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUTPUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_EROSION, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_DEPOSITION, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_FLOW_MAP, CONFIG(node));

  // attribute(s)
  // clang-format off
  node.set_current_category("Multiscale");
  add_seed(node, A_SEED, "Seed");
  add_int(node, A_LEVELS, "Levels", 3, 1, 6);
  add_float(node, A_MIX, "Mix Factor", 0.7f, 0.f, 1.f);
  add_float(node, A_WARP, "Domain Warp", 0.8f, 0.f, 2.f);

  node.set_current_category("Erosion");
  add_float(node, A_C_EROSION, "Erosion Strength", 0.1f, 0.01f, 1.f);
  add_float(node, A_TALUS_REF, "Slope Threshold", 0.1f, 0.01f, 10.f);
  add_float(node, A_SATURATION_RATIO, "Water Saturation Threshold", 1.f, 0.01f, 1.f);

  node.set_current_category("Slope");
  add_float(node, A_GRADIENT_POWER, "Influence Power", 0.6f, 0.1f, 2.f);
  add_float(node, A_GRADIENT_SCALING_RATIO, "Influence Scale", 1.f, 0.f, 1.f);
  add_float(node, A_GRADIENT_PREFILTER_RADIUS, "Prefilter Radius", 0.1f, 0.f, 0.2f);

  node.set_current_category("Deposition");
  add_float(node, A_DEPOSITION_RADIUS, "Deposition Radius", 0.1f, 0.f, 0.2f);
  add_float(node, A_DEPOSITION_SCALE_RATIO, "Sediment Amount Scale", 0.5f, 0.f, 1.f);
  // clang-format on

  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_hydraulic_stream_log_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  const auto *p_in         = node.get_value_ref<hmap::VirtualArray>(P_INPUT);
  const auto *p_bedrock    = node.get_value_ref<hmap::VirtualArray>(P_BEDROCK);
  const auto *p_moisture   = node.get_value_ref<hmap::VirtualArray>(P_MOISTURE);
  const auto *p_mask       = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  auto       *p_out        = node.get_value_ref<hmap::VirtualArray>(P_OUTPUT);
  auto       *p_erosion    = node.get_value_ref<hmap::VirtualArray>(P_EROSION);
  auto       *p_deposition = node.get_value_ref<hmap::VirtualArray>(P_DEPOSITION);
  auto       *p_flow_map   = node.get_value_ref<hmap::VirtualArray>(P_FLOW_MAP);

  if (!p_in)
    return;

  int deposition_ir = (int)(node.val<float>(A_DEPOSITION_RADIUS) * p_out->shape.x);
  int gradient_ir = (int)(node.val<float>(A_GRADIENT_PREFILTER_RADIUS) * p_out->shape.x);

  deposition_ir = std::max(1, deposition_ir);
  gradient_ir   = std::max(1, gradient_ir);

  // --- Prepare mask

  hmap::VirtualArray mask_default = pre_process_mask(node, p_mask, *p_in);
  if (!mask_default.empty())
    p_mask = &mask_default;

  // --- Compute

  float hmin = p_in->min(node.cfg().cm_cpu);
  float hmax = p_in->max(node.cfg().cm_cpu);

  if (hmax - hmin < 1.0e-6f)
  {
    Logger::log()->warn(
        "HydraulicStreamLog [{}]: input is flat/near-constant (value range ~ 0). "
        "Erosion is gradient-driven and will produce little or no change.",
        node.get_id());
    return;
  }

  hmap::for_each_tile(
      {p_in, p_mask, p_bedrock, p_moisture},
      {p_out, p_erosion, p_deposition, p_flow_map},
      [&](std::vector<const hmap::Array *> p_arrays_in,
          std::vector<hmap::Array *>       p_arrays_out,
          const hmap::TileRegion &)
      {
        auto [pa_in, pa_mask, pa_bedrock, pa_moisture]        = unpack<4>(p_arrays_in);
        auto [pa_out, pa_erosion, pa_deposition, pa_flow_map] = unpack<4>(p_arrays_out);

        *pa_out = *pa_in;

        hmap::gpu::hydraulic_stream_log_multiscale(
            *pa_out,
            node.val<float>(A_C_EROSION),
            node.val<float>(A_TALUS_REF),
            pa_mask,
            node.val<int>(A_LEVELS),
            deposition_ir,
            node.val<float>(A_DEPOSITION_SCALE_RATIO),
            node.val<float>(A_GRADIENT_POWER),
            node.val<float>(A_GRADIENT_SCALING_RATIO),
            gradient_ir,
            node.val<float>(A_SATURATION_RATIO),
            pa_bedrock,
            pa_moisture,
            pa_erosion,
            pa_deposition,
            pa_flow_map,
            node.val<float>(A_MIX),
            node.val<float>(A_WARP),
            static_cast<std::uint32_t>(node.val<int>(A_SEED)));
      },
      node.cfg().cm_gpu);

  p_out->sync_overlap_buffers();

  p_erosion->sync_overlap_buffers();
  p_erosion->remap(0.f, 1.f, node.cfg().cm_cpu);

  p_deposition->sync_overlap_buffers();
  p_deposition->remap(0.f, 1.f, node.cfg().cm_cpu);

  p_flow_map->sync_overlap_buffers();
  p_flow_map->remap(0.f, 1.f, node.cfg().cm_cpu);

  // post-process
  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
