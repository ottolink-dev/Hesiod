/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/primitives.hpp"
#include "highmap/range.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_IN   = "input";
constexpr const char *P_MASK = "mask";
constexpr const char *P_OUT  = "output";

constexpr const char *A_ANGLE                = "angle";
constexpr const char *A_APPLY_ELEVATION_MASK = "apply_elevation_mask";
constexpr const char *A_APPLY_RIDGE_MASK     = "apply_ridge_mask";
constexpr const char *A_ENABLE_RIDGE_NOISE   = "enable_ridge_noise";
constexpr const char *A_GAMMA                = "gamma";
constexpr const char *A_GAMMA_NOISE_RATIO    = "gamma_noise_ratio";
constexpr const char *A_KZ                   = "kz";
constexpr const char *A_LACUNARITY           = "lacunarity";
constexpr const char *A_LINEAR_GAMMA         = "linear_gamma";
constexpr const char *A_MASK_GAMMA           = "mask_gamma";
constexpr const char *A_NOISE_AMP            = "noise_amp";
constexpr const char *A_NOISE_KW             = "noise_kw";
constexpr const char *A_OCTAVES              = "octaves";
constexpr const char *A_RIDGE_ANGLE_SHIFT    = "ridge_angle_shift";
constexpr const char *A_RIDGE_CLAMP_VMIN     = "ridge_clamp_vmin";
constexpr const char *A_RIDGE_NOISE_AMP      = "ridge_noise_amp";
constexpr const char *A_RIDGE_NOISE_KW       = "ridge_noise_kw";
constexpr const char *A_RIDGE_REMAP_VMIN     = "ridge_remap_vmin";
constexpr const char *A_SEED                 = "seed";
constexpr const char *A_SLOPE                = "slope";

void setup_strata_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  glm::vec2 kw_default;
  add_float(node, A_ANGLE, "angle", 0.f, -180.f, 180.f);
  add_float(node, A_SLOPE, "slope", 2.f, 0.01f, 10.f);
  add_float(node, A_KZ, "kz", 1.f, 0.f, FLT_MAX);
  add_float(node, A_GAMMA, "gamma", 0.5f, 0.01f, 2.f);
  add_seed(node, A_SEED, "Seed");
  add_bool(node, A_LINEAR_GAMMA, "linear_gamma", true);
  add_int(node, A_OCTAVES, "Octaves", 4, 0, 32);
  add_float(node, A_LACUNARITY, "Lacunarity", 2.f, 0.01f, 4.f);
  add_float(node, A_GAMMA_NOISE_RATIO, "gamma_noise_ratio", 0.5f, 0.f, 1.f);
  add_float(node, A_NOISE_AMP, "noise_amp", 0.4f, 0.f, 1.f);

  kw_default = {4.f, 4.f};
  add_wavenumber(node, A_NOISE_KW, "Spatial Frequency", kw_default, 0.f, 32.f, true);

  add_bool(node, A_ENABLE_RIDGE_NOISE, "enable_ridge_noise", true);

  kw_default = {4.f, 1.5f};
  add_wavenumber(node, A_RIDGE_NOISE_KW, "ridge_noise_kw", kw_default, 0.f, 32.f, false);
  add_float(node, A_RIDGE_ANGLE_SHIFT, "ridge_angle_shift", 45.f, -180.f, 180.f);
  add_float(node, A_RIDGE_NOISE_AMP, "ridge_noise_amp", 0.4f, 0.f, 1.f);
  add_float(node, A_RIDGE_CLAMP_VMIN, "ridge_clamp_vmin", 0.5f, 0.f, 1.f);
  add_float(node, A_RIDGE_REMAP_VMIN, "ridge_remap_vmin", 0.6f, 0.f, 1.f);
  add_bool(node, A_APPLY_ELEVATION_MASK, "apply_elevation_mask", true);
  add_bool(node, A_APPLY_RIDGE_MASK, "apply_ridge_mask", true);
  add_float(node, A_MASK_GAMMA, "mask_gamma", 1.f, 0.01f, 4.f);

  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

void compute_strata_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);
    hmap::VirtualArray *p_out  = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    // prepare mask
    std::shared_ptr<hmap::VirtualArray> sp_mask = pre_process_mask(node, p_mask, *p_in);

    float hmin = p_in->min(node.cfg().cm_cpu);
    float hmax = p_in->max(node.cfg().cm_cpu);

    hmap::for_each_tile(
        {p_out, p_in, p_mask},
        [&node, hmin, hmax](std::vector<hmap::Array *> p_arrays,
                            const hmap::TileRegion    &region)
        {
          auto [pa_out, pa_in, pa_mask] = unpack<3>(p_arrays);

          *pa_out = *pa_in;

          // remap to [0, 1] as required by this filter
          hmap::remap(*pa_out, 0.f, 1.f, hmin, hmax);

          hmap::gpu::strata(*pa_out,
                            node.val<float>(A_ANGLE),
                            node.val<float>(A_SLOPE),
                            node.val<float>(A_GAMMA),
                            node.val<int>(A_SEED),
                            node.val<bool>(A_LINEAR_GAMMA),
                            node.val<float>(A_KZ),
                            node.val<int>(A_OCTAVES),
                            node.val<float>(A_LACUNARITY),
                            node.val<float>(A_GAMMA_NOISE_RATIO),
                            node.val<float>(A_NOISE_AMP),
                            node.val<glm::vec2>(A_NOISE_KW),
                            node.val<bool>(A_ENABLE_RIDGE_NOISE),
                            node.val<glm::vec2>(A_RIDGE_NOISE_KW),
                            node.val<float>(A_RIDGE_ANGLE_SHIFT),
                            node.val<float>(A_RIDGE_NOISE_AMP),
                            node.val<float>(A_RIDGE_CLAMP_VMIN),
                            node.val<float>(A_RIDGE_REMAP_VMIN),
                            node.val<bool>(A_APPLY_ELEVATION_MASK),
                            node.val<bool>(A_APPLY_RIDGE_MASK),
                            node.val<float>(A_MASK_GAMMA),
                            pa_mask,
                            region.bbox);
        },
        node.cfg().cm_gpu);

    p_out->smooth_overlap_buffers();
    p_out->remap(hmin, hmax, node.cfg().cm_cpu);

    // post-process
    post_process_heightmap(node, *p_out, p_in);
  }
}

} // namespace hesiod
