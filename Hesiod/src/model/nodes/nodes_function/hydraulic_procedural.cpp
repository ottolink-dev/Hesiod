/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
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

constexpr const char *P_INPUT         = "input";
constexpr const char *P_NOISE_X       = "noise_x";
constexpr const char *P_NOISE_Y       = "noise_y";
constexpr const char *P_MASK          = "mask";
constexpr const char *P_ANGLE_SHIFT   = "angle_shift";
constexpr const char *P_KP_MULTIPLIER = "kp_multiplier";
constexpr const char *P_OUTPUT        = "output";
constexpr const char *P_RIDGE_MASK    = "ridge_mask";

constexpr const char *A_KP_GLOBAL                 = "kp_global";
constexpr const char *A_C_EROSION                 = "c_erosion";
constexpr const char *A_OCTAVES                   = "octaves";
constexpr const char *A_PERSISTENCE               = "persistence";
constexpr const char *A_LACUNARITY                = "lacunarity";
constexpr const char *A_SEED                      = "seed";
constexpr const char *A_EROSION_PROFILE           = "erosion_profile";
constexpr const char *A_EROSION_PROFILE_PARAMETER = "erosion_profile_parameter";
constexpr const char *A_ANGLE_SHIFT               = "angle_shift";
constexpr const char *A_PHASE_SMOOTHING           = "phase_smoothing";
constexpr const char *A_TALUS_REF                 = "talus_ref";
constexpr const char *A_GRADIENT_SCALING_RATIO    = "gradient_scaling_ratio";
constexpr const char *A_GRADIENT_POWER            = "gradient_power";
constexpr const char *A_EXCLUDE_RIDGES            = "exclude_ridges";
constexpr const char *A_APPLY_DEPOSITION          = "apply_deposition";
constexpr const char *A_DEPOSITION_STRENGTH       = "deposition_strength";
constexpr const char *A_ENABLE_DEFAULT_NOISE      = "enable_default_noise";
constexpr const char *A_NOISE_AMP                 = "noise_amp";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_hydraulic_procedural_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_INPUT);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_KP_MULTIPLIER);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ANGLE_SHIFT);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_NOISE_X);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_NOISE_Y);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUTPUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_RIDGE_MASK, CONFIG(node));

  // attribute(s)
  // clang-format off
  add_float(node, A_KP_GLOBAL, "Spatial Frequency", 24.f, 0.01f, FLT_MAX);
  add_float(node, A_C_EROSION, "Erosion Strength", 0.2f, 0.f, 2.f);
  add_int(node, A_OCTAVES, "Octaves", 3, 1, 8);
  add_float(node, A_PERSISTENCE, "Persistence", 0.3f, 0.f, 1.f);
  add_float(node, A_LACUNARITY, "Lacunarity", 2.f, 0.01f, 4.f);
  add_seed(node, A_SEED, "Seed");
  add_enum(node, A_EROSION_PROFILE, "Erosion Profile Type", enum_mappings.erosion_profile_map, "Triangle Grenier");
  add_float(node, A_EROSION_PROFILE_PARAMETER, "Erosion Profile Sharpness", 0.01f, 0.001f, 1.f);
  add_float(node, A_ANGLE_SHIFT, "Slope Angle Shift", 0.f, -180.f, 180.f, "{:.0f}°");
  add_float(node, A_PHASE_SMOOTHING, "Phase Smoothing Factor", 0.1f, 0.01f, 2.f);
  add_float(node, A_TALUS_REF, "Reference Talus", 1.f, 0.f, FLT_MAX);
  add_float(node, A_GRADIENT_SCALING_RATIO, "Gradient Scaling Ratio", 1.f, 0.f, 1.f);
  add_float(node, A_GRADIENT_POWER, "Gradient Response Exponent", 0.8f, 0.01f, 4.f);
  add_bool(node, A_EXCLUDE_RIDGES, "Exclude Ridge Erosion", true);
  add_bool(node, A_APPLY_DEPOSITION, "Enable Sediment Deposition", true);
  add_float(node, A_DEPOSITION_STRENGTH, "Deposition Strength", 1.f, 0.f, 1.f);
  add_bool(node, A_ENABLE_DEFAULT_NOISE, "Enable Base Noise", true);
  add_float(node, A_NOISE_AMP, "Noise Amplitude", 0.005f, 0.0001f, 0.1f, "{:.2e}", true);
  // clang-format on

  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_hydraulic_procedural_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto p_in            = node.get_value_ref<hmap::VirtualArray>(P_INPUT);
  auto p_noise_x       = node.get_value_ref<hmap::VirtualArray>(P_NOISE_X);
  auto p_noise_y       = node.get_value_ref<hmap::VirtualArray>(P_NOISE_Y);
  auto p_mask          = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  auto p_angle_shift   = node.get_value_ref<hmap::VirtualArray>(P_ANGLE_SHIFT);
  auto p_kp_multiplier = node.get_value_ref<hmap::VirtualArray>(P_KP_MULTIPLIER);
  auto p_out           = node.get_value_ref<hmap::VirtualArray>(P_OUTPUT);
  auto p_ridge_mask    = node.get_value_ref<hmap::VirtualArray>(P_RIDGE_MASK);

  if (!p_in)
    return;

  // --- Parameters wrapper

  const auto params = [&node, p_in]()
  {
    struct P
    {
      float                kp_global;
      float                c_erosion;
      int                  octaves;
      float                persistence;
      float                lacunarity;
      uint                 seed;
      hmap::ErosionProfile erosion_profile;
      float                erosion_profile_parameter;
      float                angle_shift;
      float                phase_smoothing;
      float                talus;
      float                gradient_scaling_ratio;
      float                gradient_power;
      bool                 exclude_ridges;
      bool                 apply_deposition;
      float                deposition_strength;
      bool                 enable_default_noise;
      float                noise_amp;
    };

    const int   nx        = p_in->shape.x;
    const float talus_ref = node.val<float>(A_TALUS_REF);
    const float talus     = talus_ref / float(nx);
    const auto  ep        = hmap::ErosionProfile(node.val<int>(A_EROSION_PROFILE));

    return P{
        .kp_global                 = node.val<float>(A_KP_GLOBAL),
        .c_erosion                 = node.val<float>(A_C_EROSION),
        .octaves                   = node.val<int>(A_OCTAVES),
        .persistence               = node.val<float>(A_PERSISTENCE),
        .lacunarity                = node.val<float>(A_LACUNARITY),
        .seed                      = uint(node.val<int>(A_SEED)),
        .erosion_profile           = ep,
        .erosion_profile_parameter = node.val<float>(A_EROSION_PROFILE_PARAMETER),
        .angle_shift               = node.val<float>(A_ANGLE_SHIFT),
        .phase_smoothing           = node.val<float>(A_PHASE_SMOOTHING),
        .talus                     = talus,
        .gradient_scaling_ratio    = node.val<float>(A_GRADIENT_SCALING_RATIO),
        .gradient_power            = node.val<float>(A_GRADIENT_POWER),
        .exclude_ridges            = node.val<bool>(A_EXCLUDE_RIDGES),
        .apply_deposition          = node.val<bool>(A_APPLY_DEPOSITION),
        .deposition_strength       = node.val<float>(A_DEPOSITION_STRENGTH),
        .enable_default_noise      = node.val<bool>(A_ENABLE_DEFAULT_NOISE),
        .noise_amp                 = node.val<float>(A_NOISE_AMP),
    };
  }();

  // --- Prepare mask

  std::shared_ptr<hmap::VirtualArray> sp_mask = pre_process_mask(node, p_mask, *p_in);

  // --- Compute

  hmap::for_each_tile(
      {p_in, p_noise_x, p_noise_y, p_mask, p_angle_shift, p_kp_multiplier},
      {p_out, p_ridge_mask},
      [&node, &params](std::vector<const hmap::Array *> p_arrays_in,
                       std::vector<hmap::Array *>       p_arrays_out,
                       const hmap::TileRegion          &region)
      {
        const auto [pa_in,
                    pa_noise_x,
                    pa_noise_y,
                    pa_mask,
                    pa_angle_shift,
                    pa_kp_multiplier] = unpack<6>(p_arrays_in);
        auto [pa_out, pa_ridge_mask]  = unpack<2>(p_arrays_out);

        *pa_out = *pa_in;

        hmap::gpu::hydraulic_procedural_fbm(*pa_out,
                                            params.kp_global,
                                            params.c_erosion,
                                            params.seed,
                                            pa_mask,
                                            params.erosion_profile,
                                            params.octaves,
                                            params.persistence,
                                            params.lacunarity,
                                            params.erosion_profile_parameter,
                                            params.angle_shift,
                                            params.phase_smoothing,
                                            params.talus,
                                            params.gradient_scaling_ratio,
                                            params.gradient_power,
                                            params.exclude_ridges,
                                            params.apply_deposition,
                                            params.deposition_strength,
                                            params.enable_default_noise,
                                            params.noise_amp,
                                            pa_kp_multiplier,
                                            pa_angle_shift,
                                            pa_noise_x,
                                            pa_noise_y,
                                            pa_ridge_mask,
                                            region.bbox);
      },
      node.cfg().cm_gpu);

  // post-process
  p_out->smooth_overlap_buffers();
  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
