/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/primitives.hpp"

#include "hesiod/model/nodes/attributes.hpp"

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

constexpr const char *P_INPUT   = "input";
constexpr const char *P_MASK    = "mask";
constexpr const char *P_NOISE_X = "noise_x";
constexpr const char *P_NOISE_Y = "noise_y";
constexpr const char *P_OUTPUT  = "output";

constexpr const char *A_KW                    = "kw";
constexpr const char *A_AMP                   = "amp";
constexpr const char *A_SEED                  = "seed";
constexpr const char *A_GAMMA                 = "gamma";
constexpr const char *A_GAMMA_LATERAL         = "gamma_lateral";
constexpr const char *A_ANGLE                 = "angle";
constexpr const char *A_ENABLE_DEFAULT_NOISE  = "enable_default_noise";
constexpr const char *A_NOISE_AMP             = "noise_amp";
constexpr const char *A_ABSOLUTE_DISPLACEMENT = "absolute_displacement";
constexpr const char *A_OCCURENCE_PROBABILITY = "occurence_probability";
constexpr const char *A_OCTAVES               = "octaves";
constexpr const char *A_PERSISTENCE           = "persistence";
constexpr const char *A_LACUNARITY            = "lacunarity";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_strata_cells_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_INPUT);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_NOISE_X);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_NOISE_Y);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUTPUT, CONFIG(node));

  // attribute(s)
  // clang-format off
  add_wavenumber(node, A_KW, "Spatial Frequency", glm::vec2(2.f, 6.f), 0.f, FLT_MAX, false);
  add_float(node, A_AMP, "Strata Strength", 0.2f, 0.f, 1.f);
  add_seed(node, A_SEED, "Seed");
  add_float(node, A_GAMMA, "Longitudinal Sharpness", 0.5f, 0.01f, 2.f);
  add_float(node, A_GAMMA_LATERAL, "Lateral Sharpness", 0.4f, 0.01f, 2.f);
  add_float(node, A_ANGLE, "Orientation Angle", 0.f, -180.f, 180.f, "{:.0f}°");
  add_bool(node, A_ENABLE_DEFAULT_NOISE, "Enable Base Noise", true);
  add_float(node, A_NOISE_AMP, "Base Noise Amplitude", 0.05f, 0.f, 1.f);
  add_bool(node, A_ABSOLUTE_DISPLACEMENT, "Use Absolute Displacement", false);
  add_float(node, A_OCCURENCE_PROBABILITY, "Feature Occurrence Probability", 1.f, 0.f, 1.f);
  add_int(node, A_OCTAVES, "Octaves", 6, 1, 32);
  add_float(node, A_PERSISTENCE, "Persistence", 0.5f, 0.f, 1.f);
  add_float(node, A_LACUNARITY, "Lacunarity", 2.2f, 0.01f, 4.f);
  // clang-format on

  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_strata_cells_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto *p_in   = node.get_value_ref<hmap::VirtualArray>(P_INPUT);
  auto *p_dx   = node.get_value_ref<hmap::VirtualArray>(P_NOISE_X);
  auto *p_dy   = node.get_value_ref<hmap::VirtualArray>(P_NOISE_Y);
  auto *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  auto *p_out  = node.get_value_ref<hmap::VirtualArray>(P_OUTPUT);

  if (!p_in)
    return;

  // --- Params wrapper

  const auto params = [&node]()
  {
    struct P
    {
      glm::vec2 kw;
      float     amp;
      uint      seed;
      float     gamma;
      float     gamma_lateral;
      float     angle;
      bool      enable_default_noise;
      float     noise_amp;
      bool      absolute_displacement;
      float     occurence_probability;
      int       octaves;
      float     persistence;
      float     lacunarity;
    };

    return P{
        .kw                    = node.val<glm::vec2>(A_KW),
        .amp                   = node.val<float>(A_AMP),
        .seed                  = uint(node.val<int>(A_SEED)),
        .gamma                 = node.val<float>(A_GAMMA),
        .gamma_lateral         = node.val<float>(A_GAMMA_LATERAL),
        .angle                 = node.val<float>(A_ANGLE),
        .enable_default_noise  = node.val<bool>(A_ENABLE_DEFAULT_NOISE),
        .noise_amp             = node.val<float>(A_NOISE_AMP),
        .absolute_displacement = node.val<bool>(A_ABSOLUTE_DISPLACEMENT),
        .occurence_probability = node.val<float>(A_OCCURENCE_PROBABILITY),
        .octaves               = node.val<int>(A_OCTAVES),
        .persistence           = node.val<float>(A_PERSISTENCE),
        .lacunarity            = node.val<float>(A_LACUNARITY),
    };
  }();

  // --- Prepare mask

  std::shared_ptr<hmap::VirtualArray> sp_mask = pre_process_mask(node, p_mask, *p_in);

  // --- Compute

  float hmin = p_in->min(node.cfg().cm_cpu);
  float hmax = p_in->max(node.cfg().cm_cpu);

  hmap::for_each_tile(
      {p_in, p_dx, p_dy, p_mask},
      {p_out},
      [&node, &params](std::vector<const hmap::Array *> p_arrays_in,
                       std::vector<hmap::Array *>       p_arrays_out,
                       const hmap::TileRegion          &region)
      {
        const auto [pa_in, pa_dx, pa_dy, pa_mask] = unpack<4>(p_arrays_in);
        auto [pa_out]                             = unpack<1>(p_arrays_out);

        *pa_out = *pa_in;

        hmap::gpu::strata_cells_fbm(*pa_out,
                                    params.kw,
                                    params.amp,
                                    params.seed,
                                    pa_mask,
                                    params.gamma,
                                    params.gamma_lateral,
                                    params.angle,
                                    params.enable_default_noise,
                                    params.noise_amp,
                                    params.absolute_displacement,
                                    params.occurence_probability,
                                    params.octaves,
                                    params.persistence,
                                    params.lacunarity,
                                    pa_dx,
                                    pa_dy,
                                    region.bbox);
      },
      node.cfg().cm_gpu);

  p_out->remap(hmin, hmax, node.cfg().cm_cpu);

  // post-process
  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
