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
constexpr const char *P_CONTROL  = "control";
constexpr const char *P_DX       = "dx";
constexpr const char *P_DY       = "dy";
constexpr const char *P_ENVELOPE = "envelope";
constexpr const char *P_OUT      = "output";

constexpr const char *A_DENSITY       = "density";
constexpr const char *A_KW            = "kw";
constexpr const char *A_KW_MULTIPLIER = "kw_multiplier";
constexpr const char *A_LACUNARITY    = "lacunarity";
constexpr const char *A_OCTAVES       = "octaves";
constexpr const char *A_PERSISTENCE   = "persistence";
constexpr const char *A_SEED          = "seed";
constexpr const char *A_VORTICITY     = "vorticity";
constexpr const char *A_WEIGHT        = "weight";

void setup_wavelet_noise_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_CONTROL);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ENVELOPE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_wavenumber(node, A_KW, "Spatial Frequency");
  add_seed(node, A_SEED, "Seed");
  add_float(node, A_KW_MULTIPLIER, "kw_multiplier", 2.f, 0.f, FLT_MAX);
  add_float(node, A_VORTICITY, "vorticity", 0.f, 0.f, 10.f);
  add_float(node, A_DENSITY, "density", 1.f, 0.f, 1.f);
  add_int(node, A_OCTAVES, "Octaves", 8, 0, 32);
  add_float(node, A_WEIGHT, "Weight", 0.7f, 0.f, 1.f);
  add_float(node, A_PERSISTENCE, "Persistence", 0.5f, 0.f, 1.f);
  add_float(node, A_LACUNARITY, "Lacunarity", 2.f, 0.01f, 4.f);

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

void compute_wavelet_noise_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // base noise function
  hmap::VirtualArray *p_dx   = node.get_value_ref<hmap::VirtualArray>(P_DX);
  hmap::VirtualArray *p_dy   = node.get_value_ref<hmap::VirtualArray>(P_DY);
  hmap::VirtualArray *p_ctrl = node.get_value_ref<hmap::VirtualArray>(P_CONTROL);
  hmap::VirtualArray *p_env  = node.get_value_ref<hmap::VirtualArray>(P_ENVELOPE);
  hmap::VirtualArray *p_out  = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  hmap::for_each_tile(
      {p_out, p_dx, p_dy, p_ctrl},
      [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
      {
        auto [pa_out, pa_dx, pa_dy, pa_ctrl] = unpack<4>(p_arrays);

        *pa_out = hmap::gpu::wavelet_noise(region.shape,
                                           node.val<glm::vec2>(A_KW),
                                           node.val<int>(A_SEED),
                                           node.val<float>(A_KW_MULTIPLIER),
                                           node.val<float>(A_VORTICITY),
                                           node.val<float>(A_DENSITY),
                                           node.val<int>(A_OCTAVES),
                                           node.val<float>(A_WEIGHT),
                                           node.val<float>(A_PERSISTENCE),
                                           node.val<float>(A_LACUNARITY),
                                           pa_ctrl,
                                           pa_dx,
                                           pa_dy,
                                           region.bbox);
      },
      node.cfg().cm_gpu);

  // post-process
  post_apply_enveloppe(node, *p_out, p_env);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
