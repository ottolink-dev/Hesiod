/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/primitives.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_DX = "dx";
constexpr const char *P_DY = "dy";
constexpr const char *P_ENV = "envelope";
constexpr const char *P_OUT = "out";

constexpr const char *A_KW = "kw";
constexpr const char *A_SEED = "seed";
constexpr const char *A_WARP_KW = "warp_kw";
constexpr const char *A_LARGE_SCALE_WARP_AMP = "large_scale_warp_amp";
constexpr const char *A_LARGE_SCALE_GAIN = "large_scale_gain";
constexpr const char *A_LARGE_SCALE_AMP = "large_scale_amp";
constexpr const char *A_MEDIUM_SCALE_KW_RATIO = "medium_scale_kw_ratio";
constexpr const char *A_MEDIUM_SCALE_WARP_AMP = "medium_scale_warp_amp";
constexpr const char *A_MEDIUM_SCALE_GAIN = "medium_scale_gain";
constexpr const char *A_MEDIUM_SCALE_AMP = "medium_scale_amp";
constexpr const char *A_SMALL_SCALE_KW_RATIO = "small_scale_kw_ratio";
constexpr const char *A_SMALL_SCALE_AMP = "small_scale_amp";
constexpr const char *A_SMALL_SCALE_OVERLAY_AMP = "small_scale_overlay_amp";
constexpr const char *A_RUGOSITY_KW_RATIO = "rugosity_kw_ratio";
constexpr const char *A_RUGOSITY_AMP = "rugosity_amp";
constexpr const char *A_FLATTEN_ACTIVATE = "flatten_activate";
constexpr const char *A_FLATTEN_KW_RATIO = "flatten_kw_ratio";
constexpr const char *A_FLATTEN_AMP = "flatten_amp";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_basalt_field_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ENV);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  glm::vec2 kw = {5.f, 5.f};
  add_wavenumber(node, A_KW, "Spatial Frequency", kw, 0.f, FLT_MAX, true);
  add_seed(node, A_SEED, "Seed");

  node.set_current_category("Large-Scale Parameters");
  add_float(node, A_WARP_KW, "warp_kw", 4.f, 0.f, FLT_MAX);
  add_float(node, A_LARGE_SCALE_WARP_AMP, "large_scale_warp_amp", 0.2f, 0.f, 1.f);
  add_float(node, A_LARGE_SCALE_GAIN, "large_scale_gain", 8.f, 0.f, 10.f);
  add_float(node, A_LARGE_SCALE_AMP, "large_scale_amp", 0.15f, 0.f, 1.f);

  node.set_current_category("Medium-Scale Parameters");
  add_float(node, A_MEDIUM_SCALE_KW_RATIO, "medium_scale_kw_ratio", 3.f, 0.f, FLT_MAX);
  add_float(node, A_MEDIUM_SCALE_WARP_AMP, "medium_scale_warp_amp", 0.1f, 0.f, 1.f);
  add_float(node, A_MEDIUM_SCALE_GAIN, "medium_scale_gain", 8.f, 0.f, 10.f);
  add_float(node, A_MEDIUM_SCALE_AMP, "medium_scale_amp", 0.12f, 0.f, 0.2f);

  node.set_current_category("Small-Scale Parameters");
  add_float(node, A_SMALL_SCALE_KW_RATIO, "small_scale_kw_ratio", 5.f, 0.f, FLT_MAX);
  add_float(node, A_SMALL_SCALE_AMP, "small_scale_amp", 0.1f, 0.f, 1.f);
  add_float(node,
            A_SMALL_SCALE_OVERLAY_AMP,
            "small_scale_overlay_amp",
            0.005f,
            0.f,
            0.01f);

  node.set_current_category("Rugosity");
  add_float(node, A_RUGOSITY_KW_RATIO, "rugosity_kw_ratio", 1.5f, 0.f, FLT_MAX);
  add_float(node, A_RUGOSITY_AMP, "rugosity_amp", 1.f, 0.f, 2.f);

  node.set_current_category("Flattening");
  add_bool(node, A_FLATTEN_ACTIVATE, "flatten_activate", true);
  add_float(node, A_FLATTEN_KW_RATIO, "flatten_kw_ratio", 0.5f, 0.f, FLT_MAX);
  add_float(node, A_FLATTEN_AMP, "flatten_amp", 0.f, 0.f, 1.f);

  setup_post_process_heightmap_attributes(
      node,
      {.add_mix = false, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_basalt_field_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_dx = node.get_value_ref<hmap::VirtualArray>(P_DX);
  auto *p_dy = node.get_value_ref<hmap::VirtualArray>(P_DY);
  auto *p_env = node.get_value_ref<hmap::VirtualArray>(P_ENV);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_out)
    return;

  // --- Params

  const auto kw = node.val<glm::vec2>(A_KW);
  const auto seed = static_cast<uint>(node.val<int>(A_SEED));
  const auto warp_kw = node.val<float>(A_WARP_KW);
  const auto large_scale_warp_amp = node.val<float>(A_LARGE_SCALE_WARP_AMP);
  const auto large_scale_gain = node.val<float>(A_LARGE_SCALE_GAIN);
  const auto large_scale_amp = node.val<float>(A_LARGE_SCALE_AMP);
  const auto medium_scale_kw_ratio = node.val<float>(A_MEDIUM_SCALE_KW_RATIO);
  const auto medium_scale_warp_amp = node.val<float>(A_MEDIUM_SCALE_WARP_AMP);
  const auto medium_scale_gain = node.val<float>(A_MEDIUM_SCALE_GAIN);
  const auto medium_scale_amp = node.val<float>(A_MEDIUM_SCALE_AMP);
  const auto small_scale_kw_ratio = node.val<float>(A_SMALL_SCALE_KW_RATIO);
  const auto small_scale_amp = node.val<float>(A_SMALL_SCALE_AMP);
  const auto small_scale_overlay_amp = node.val<float>(A_SMALL_SCALE_OVERLAY_AMP);
  const auto rugosity_kw_ratio = node.val<float>(A_RUGOSITY_KW_RATIO);
  const auto rugosity_amp = node.val<float>(A_RUGOSITY_AMP);
  const auto flatten_activate = node.val<bool>(A_FLATTEN_ACTIVATE);
  const auto flatten_kw_ratio = node.val<float>(A_FLATTEN_KW_RATIO);
  const auto flatten_amp = node.val<float>(A_FLATTEN_AMP);

  // --- Compute

  hmap::for_each_tile(
      {p_out, p_dx, p_dy},
      [&](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
      {
        auto [pa_out, pa_dx, pa_dy] = unpack<3>(p_arrays);

        *pa_out = hmap::gpu::basalt_field(region.shape,
                                          kw,
                                          seed,
                                          warp_kw,
                                          large_scale_warp_amp,
                                          large_scale_gain,
                                          large_scale_amp,
                                          medium_scale_kw_ratio,
                                          medium_scale_warp_amp,
                                          medium_scale_gain,
                                          medium_scale_amp,
                                          small_scale_kw_ratio,
                                          small_scale_amp,
                                          small_scale_overlay_amp,
                                          rugosity_kw_ratio,
                                          rugosity_amp,
                                          flatten_activate,
                                          flatten_kw_ratio,
                                          flatten_amp,
                                          pa_dx,
                                          pa_dy,
                                          region.bbox);
      },
      node.cfg().cm_gpu);

  // --- Post-process

  post_apply_enveloppe(node, *p_out, p_env);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
