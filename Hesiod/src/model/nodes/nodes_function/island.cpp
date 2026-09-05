/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
   Public License. The full license is in the file LICENSE, distributed with
   this software. */

#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/primitives.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_LAND  = "land_mask";
constexpr const char *P_DR    = "dr";
constexpr const char *P_OUT   = "output";
constexpr const char *P_DEPTH = "water_depth";
constexpr const char *P_MASK  = "inland_mask";

constexpr const char *A_SEED                   = "seed";
constexpr const char *A_NOISE_AMP              = "noise_amp";
constexpr const char *A_NOISE_KW               = "noise_kw";
constexpr const char *A_NOISE_OCTAVES          = "noise_octaves";
constexpr const char *A_NOISE_RUGOSITY         = "noise_rugosity";
constexpr const char *A_NOISE_ANGLE            = "noise_angle";
constexpr const char *A_NOISE_K_SMOOTHING      = "noise_k_smoothing";
constexpr const char *A_ELEVATION_SCALE        = "elevation_scale";
constexpr const char *A_FILTER_RADIUS          = "filter_radius";
constexpr const char *A_SLOPE_MIN              = "slope_min";
constexpr const char *A_SLOPE_MAX              = "slope_max";
constexpr const char *A_SLOPE_START            = "slope_start";
constexpr const char *A_SLOPE_END              = "slope_end";
constexpr const char *A_SLOPE_NOISE_INTENSITY  = "slope_noise_intensity";
constexpr const char *A_K_SMOOTH               = "k_smooth";
constexpr const char *A_RADIAL_NOISE_INTENSITY = "radial_noise_intensity";
constexpr const char *A_RADIAL_PROFILE_GAIN    = "radial_profile_gain";
constexpr const char *A_WATER_DECAY            = "water_decay";
constexpr const char *A_WATER_DEPTH            = "water_depth";
constexpr const char *A_LEE_ANGLE              = "lee_angle";
constexpr const char *A_LEE_AMP                = "lee_amp";
constexpr const char *A_UPLIFT_AMP             = "uplift_amp";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_island_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_LAND);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DR);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_DEPTH, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_MASK, CONFIG(node));

  // attribute(s)

  node.set_current_category("Island");

  node.set_current_category("Core Parameters");
  add_float(node, A_ELEVATION_SCALE, "Elevation Scaling Factor", 0.4f, 0.f, 2.f);
  add_float(node, A_FILTER_RADIUS, "Smoothing Radius", 0.01f, 0.f, 0.2f);
  add_float(node, A_SLOPE_START, "Shoreline Slope", 0.5f, 0.f, 4.f);
  add_float(node, A_SLOPE_END, "Apex Slope", 1.5f, 0.f, 4.f);
  add_float(node, A_RADIAL_PROFILE_GAIN, "Radial Profile Gain", 1.f, 0.01f, 10.f);

  node.set_current_category("Large-Scale Deformations");
  add_float(node, A_LEE_ANGLE, "Lee-Side Angle", 30.f, -180.f, 180.f);
  add_float(node, A_LEE_AMP, "Lee-Side Amplitude", 0.f, 0.f, 1.f);
  add_float(node, A_UPLIFT_AMP, "Uplift Amplitude", 0.f, 0.f, 1.f);

  node.set_current_category("Water Parameters");
  add_float(node, A_WATER_DECAY, "Water Decay Rate", 0.1f, 0.f, 1.f);
  add_float(node, A_WATER_DEPTH, "Water Depth Level", 0.3f, 0.f, FLT_MAX);

  node.set_current_category("Morphology Controls");
  add_float(node, A_SLOPE_MIN, "Lower Slope Limit", 0.05f, 0.f, 32.f);
  add_float(node, A_SLOPE_MAX, "Upper Slope Limit", 1.5f, 0.f, 32.f);
  add_float(node, A_SLOPE_NOISE_INTENSITY, "Slope Noise Intensity", 0.f, 0.f, 100.f);
  add_float(node, A_K_SMOOTH, "Smoothing Coefficient", 0.05f, 0.f, 1.f);

  node.set_current_category("Noise Generation");
  add_seed(node, A_SEED, "Seed");
  add_float(node, A_NOISE_AMP, "Amplitude", 0.07f, 0.f, 2.f);
  add_wavenumber(node, A_NOISE_KW, "Spatial Frequency", {4.f, 4.f}, 0.f, FLT_MAX, true);
  add_int(node, A_NOISE_OCTAVES, "Octaves", 8, 0, INT_MAX);
  add_float(node, A_NOISE_RUGOSITY, "Rugosity", 0.7f, 0.f, 1.f);
  add_float(node, A_NOISE_ANGLE, "Direction Angle", 45.f, -180.f, 180.f);
  add_float(node, A_NOISE_K_SMOOTHING, "Smoothing Coefficient", 0.05f, 0.f, 1.f);
  add_float(node, A_RADIAL_NOISE_INTENSITY, "Radial Noise Intensity", 0.5f, 0.f, 2.f);

  setup_post_process_heightmap_attributes(
      node,
      {.add_mix = false, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_island_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto *p_land  = node.get_value_ref<hmap::VirtualArray>(P_LAND);
  auto *p_dr    = node.get_value_ref<hmap::VirtualArray>(P_DR);
  auto *p_out   = node.get_value_ref<hmap::VirtualArray>(P_OUT);
  auto *p_depth = node.get_value_ref<hmap::VirtualArray>(P_DEPTH);
  auto *p_mask  = node.get_value_ref<hmap::VirtualArray>(P_MASK);

  if (!p_land)
    return;

  const auto seed                   = node.val<int>(A_SEED);
  const auto noise_amp              = node.val<float>(A_NOISE_AMP);
  const auto noise_kw               = node.val<glm::vec2>(A_NOISE_KW);
  const auto noise_octaves          = node.val<int>(A_NOISE_OCTAVES);
  const auto noise_rugosity         = node.val<float>(A_NOISE_RUGOSITY);
  const auto noise_angle            = node.val<float>(A_NOISE_ANGLE);
  const auto noise_k_smoothing      = node.val<float>(A_NOISE_K_SMOOTHING);
  const auto scale                  = node.val<float>(A_ELEVATION_SCALE);
  const auto slope_min              = node.val<float>(A_SLOPE_MIN);
  const auto slope_max              = node.val<float>(A_SLOPE_MAX);
  const auto slope_start            = node.val<float>(A_SLOPE_START);
  const auto slope_end              = node.val<float>(A_SLOPE_END);
  const auto slope_noise_intensity  = node.val<float>(A_SLOPE_NOISE_INTENSITY);
  const auto k_smooth               = node.val<float>(A_K_SMOOTH);
  const auto radial_noise_intensity = node.val<float>(A_RADIAL_NOISE_INTENSITY);
  const auto radial_profile_gain    = node.val<float>(A_RADIAL_PROFILE_GAIN);
  const auto water_decay            = node.val<float>(A_WATER_DECAY);
  const auto water_depth            = node.val<float>(A_WATER_DEPTH);
  const auto lee_angle              = node.val<float>(A_LEE_ANGLE);
  const auto lee_amp                = node.val<float>(A_LEE_AMP);
  const auto uplift_amp             = node.val<float>(A_UPLIFT_AMP);

  const auto ir = static_cast<int>(node.val<float>(A_FILTER_RADIUS) * p_out->shape.x);

  hmap::for_each_tile(
      {p_out, p_land, p_dr, p_depth, p_mask},
      [&](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
      {
        auto [pa_out, pa_land, pa_dr, pa_depth, pa_mask] = unpack<5>(p_arrays);

        if (pa_dr)
        {
          *pa_out = hmap::gpu::island(*pa_land,
                                      pa_dr,
                                      scale,
                                      (ir != 0),
                                      ir,
                                      scale * slope_min,
                                      scale * slope_max,
                                      scale * slope_start,
                                      scale * slope_end,
                                      slope_noise_intensity,
                                      k_smooth,
                                      radial_noise_intensity,
                                      radial_profile_gain,
                                      water_decay,
                                      scale * water_depth,
                                      lee_angle,
                                      scale * lee_amp,
                                      scale * uplift_amp,
                                      pa_depth,
                                      pa_mask);
        }
        else
        {
          *pa_out = hmap::gpu::island(*pa_land,
                                      seed,
                                      noise_amp,
                                      noise_kw,
                                      noise_octaves,
                                      noise_rugosity,
                                      noise_angle,
                                      noise_k_smoothing,
                                      scale,
                                      (ir != 0),
                                      ir,
                                      scale * slope_min,
                                      scale * slope_max,
                                      scale * slope_start,
                                      scale * slope_end,
                                      slope_noise_intensity,
                                      k_smooth,
                                      radial_noise_intensity,
                                      radial_profile_gain,
                                      water_decay,
                                      scale * water_depth,
                                      lee_angle,
                                      scale * lee_amp,
                                      scale * uplift_amp,
                                      pa_depth,
                                      pa_mask);
        }
      },
      node.cfg().cm_gpu);

  p_out->smooth_overlap_buffers();
  p_depth->smooth_overlap_buffers();
  p_mask->smooth_overlap_buffers();

  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
