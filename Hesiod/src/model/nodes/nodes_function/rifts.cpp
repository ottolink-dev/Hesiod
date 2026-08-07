/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/range.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_DX   = "dx";
constexpr const char *P_DY   = "dy";
constexpr const char *P_IN   = "input";
constexpr const char *P_MASK = "mask";
constexpr const char *P_OUT  = "output";

constexpr const char *A_AMPLITUDE             = "amplitude";
constexpr const char *A_ANGLE                 = "angle";
constexpr const char *A_APPLY_MASK            = "apply_mask";
constexpr const char *A_CENTER                = "center";
constexpr const char *A_CLAMP_VMIN            = "clamp_vmin";
constexpr const char *A_ELEVATION_NOISE_AMP   = "elevation_noise_amp";
constexpr const char *A_ELEVATION_NOISE_SHIFT = "elevation_noise_shift";
constexpr const char *A_K_SMOOTH_BOTTOM       = "k_smooth_bottom";
constexpr const char *A_K_SMOOTH_TOP          = "k_smooth_top";
constexpr const char *A_KW                    = "kw";
constexpr const char *A_MASK_GAMMA            = "mask_gamma";
constexpr const char *A_RADIAL_SPREAD_AMP     = "radial_spread_amp";
constexpr const char *A_REMAP_VMIN            = "remap_vmin";
constexpr const char *A_REVERSE_MASK          = "reverse_mask";
constexpr const char *A_SEED                  = "seed";

void setup_rifts_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  glm::vec2 kw_default = {4.f, 1.2f};
  add_wavenumber(node, A_KW, "Spatial Frequency", kw_default, 0.f, 32.f, false);
  add_float(node, A_ANGLE, "angle", 0.f, -180.f, 180.f);
  add_float(node, A_AMPLITUDE, "amplitude", 0.1f, 0.f, 1.f);
  add_seed(node, A_SEED, "Seed");
  add_float(node, A_K_SMOOTH_BOTTOM, "k_smooth_bottom", 0.05f, 0.f, 0.3f);
  add_float(node, A_K_SMOOTH_TOP, "k_smooth_top", 0.05f, 0.f, 0.3f);
  add_float(node, A_RADIAL_SPREAD_AMP, "radial_spread_amp", 0.2f, -1.f, 1.f);
  add_float(node, A_ELEVATION_NOISE_AMP, "elevation_noise_amp", 0.1f, 0.f, 1.f);
  add_float(node, A_CLAMP_VMIN, "clamp_vmin", 0.5f, 0.f, 1.f);
  add_float(node, A_REMAP_VMIN, "remap_vmin", 0.6f, 0.f, 1.f);
  add_float(node, A_ELEVATION_NOISE_SHIFT, "elevation_noise_shift", 0.f, -1.f, 1.f);
  add_bool(node, A_APPLY_MASK, "apply_mask", true);
  add_bool(node, A_REVERSE_MASK, "reverse_mask", false);
  add_float(node, A_MASK_GAMMA, "mask_gamma", 1.f, 0.01f, 4.f);
  add_xy(node, A_CENTER, "center");

  setup_default_noise(node, {.noise_amp = 0.1f, .kw = 4.f});
  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

void compute_rifts_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_dx   = node.get_value_ref<hmap::VirtualArray>(P_DX);
    hmap::VirtualArray *p_dy   = node.get_value_ref<hmap::VirtualArray>(P_DY);
    hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);
    hmap::VirtualArray *p_out  = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    // prepare mask
    std::shared_ptr<hmap::VirtualArray> sp_mask = pre_process_mask(node, p_mask, *p_in);

    // prepare default noise
    hmap::VirtualArray noise_default(CONFIG(node));
    generate_noise(node, p_dx, noise_default);

    // remap to [0, 1] as required by this filter
    float hmin = p_in->min(node.cfg().cm_cpu);
    float hmax = p_in->max(node.cfg().cm_cpu);

    hmap::for_each_tile(
        {p_out, p_in, p_dx, p_dy, p_mask},
        [&node, hmin, hmax](std::vector<hmap::Array *> p_arrays,
                            const hmap::TileRegion    &region)
        {
          auto [pa_out, pa_in, pa_dx, pa_dy, pa_mask] = unpack<5>(p_arrays);

          *pa_out = *pa_in;

          hmap::remap(*pa_out, 0.f, 1.f, hmin, hmax);

          hmap::gpu::rifts(*pa_out,
                           node.val<glm::vec2>(A_KW),
                           node.val<float>(A_ANGLE),
                           node.val<float>(A_AMPLITUDE),
                           node.val<int>(A_SEED),
                           node.val<float>(A_ELEVATION_NOISE_SHIFT),
                           node.val<float>(A_K_SMOOTH_BOTTOM),
                           node.val<float>(A_K_SMOOTH_TOP),
                           node.val<float>(A_RADIAL_SPREAD_AMP),
                           node.val<float>(A_ELEVATION_NOISE_AMP),
                           node.val<float>(A_CLAMP_VMIN),
                           node.val<float>(A_REMAP_VMIN),
                           node.val<bool>(A_APPLY_MASK),
                           !node.val<bool>(A_REVERSE_MASK) &&
                               node.val<bool>(A_APPLY_MASK),
                           node.val<float>(A_MASK_GAMMA),
                           pa_dx,
                           pa_dy,
                           pa_mask,
                           node.val<glm::vec2>(A_CENTER),
                           region.bbox);
          hmap::remap(*pa_out, hmin, hmax, 0.f, 1.f);
        },
        node.cfg().cm_gpu);

    p_out->smooth_overlap_buffers();

    // remap to original range
    p_out->remap(hmin, hmax, node.cfg().cm_cpu);

    // post-process
    post_process_heightmap(node, *p_out, p_in);
  }
}

} // namespace hesiod
