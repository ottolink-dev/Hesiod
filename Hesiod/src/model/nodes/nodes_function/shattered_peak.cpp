/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
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
constexpr const char *P_DX       = "dx";
constexpr const char *P_DY       = "dy";
constexpr const char *P_ENVELOPE = "envelope";
constexpr const char *P_OUT      = "out";

constexpr const char *A_ADD_DEPOSITION = "add_deposition";
constexpr const char *A_ANGLE          = "angle";
constexpr const char *A_BASE_NOISE_AMP = "base_noise_amp";
constexpr const char *A_BULK_AMP       = "bulk_amp";
constexpr const char *A_CENTER         = "center";
constexpr const char *A_ELEVATION      = "elevation";
constexpr const char *A_GAMMA          = "gamma";
constexpr const char *A_K_SMOOTHING    = "k_smoothing";
constexpr const char *A_OCTAVES        = "octaves";
constexpr const char *A_PEAK_KW        = "peak_kw";
constexpr const char *A_POST_REMAP     = "post_remap";
constexpr const char *A_RUGOSITY       = "rugosity";
constexpr const char *A_SCALE          = "scale";
constexpr const char *A_SEED           = "seed";

void setup_shattered_peak_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ENVELOPE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_ELEVATION, "elevation", 0.7f, 0.f, 1.f);
  add_float(node, A_SCALE, "scale", 1.f, 0.01f, FLT_MAX);
  add_seed(node, A_SEED, "Seed");
  add_int(node, A_OCTAVES, "Octaves", 8, 0, 32);
  add_float(node, A_PEAK_KW, "peak_kw", 4.f, 0.01f, FLT_MAX);
  add_float(node, A_RUGOSITY, "rugosity", 0.f, 0.f, 1.f);
  add_float(node, A_ANGLE, "angle", 45.f, -180.f, 180.f, "{:.0f}°");
  add_float(node, A_GAMMA, "gamma", 1.f, 0.01f, 4.f);
  add_bool(node, A_ADD_DEPOSITION, "add_deposition", true);
  add_float(node, A_BULK_AMP, "bulk_amp", 0.25f, 0.f, 2.f);
  add_float(node, A_BASE_NOISE_AMP, "base_noise_amp", 0.15f, 0.f, 1.f);
  add_float(node, A_K_SMOOTHING, "k_smoothing", 0.1f, 0.f, 1.f);
  add_xy(node, A_CENTER, "center");

  setup_post_process_heightmap_attributes(
      node,
      {.add_mix = false, .remap_active_state = false});
}

void compute_shattered_peak_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // base shattered_peak function
  hmap::VirtualArray *p_dx  = node.get_value_ref<hmap::VirtualArray>(P_DX);
  hmap::VirtualArray *p_dy  = node.get_value_ref<hmap::VirtualArray>(P_DY);
  hmap::VirtualArray *p_env = node.get_value_ref<hmap::VirtualArray>(P_ENVELOPE);
  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  hmap::for_each_tile(
      {p_out, p_dx, p_dy},
      [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
      {
        auto [pa_out, pa_dx, pa_dy] = unpack<3>(p_arrays);

        *pa_out = hmap::gpu::shattered_peak(region.shape,
                                            node.val<int>(A_SEED),
                                            node.val<float>(A_SCALE),
                                            node.val<int>(A_OCTAVES),
                                            node.val<float>(A_PEAK_KW),
                                            node.val<float>(A_RUGOSITY),
                                            node.val<float>(A_ANGLE),
                                            node.val<float>(A_GAMMA),
                                            node.val<bool>(A_ADD_DEPOSITION),
                                            node.val<float>(A_BULK_AMP),
                                            node.val<float>(A_BASE_NOISE_AMP),
                                            node.val<float>(A_K_SMOOTHING),
                                            node.val<glm::vec2>(A_CENTER),
                                            pa_dx,
                                            pa_dy,
                                            region.bbox);
      },
      node.cfg().cm_gpu);

  p_out->remap(0.f, node.val<float>(A_ELEVATION), node.cfg().cm_cpu);

  // post-process
  post_apply_enveloppe(node, *p_out, p_env);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
