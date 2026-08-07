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

constexpr const char *P_DX = "dx";
constexpr const char *P_DY = "dy";
constexpr const char *P_CONTROL = "control";
constexpr const char *P_ENVELOPE = "envelope";
constexpr const char *P_ANGLE = "angle";
constexpr const char *P_OUTPUT = "output";

constexpr const char *A_KW = "kw";
constexpr const char *A_ANGLE = "angle";
constexpr const char *A_ANGLE_SPREAD_RATIO = "angle_spread_ratio";
constexpr const char *A_SEED = "seed";
constexpr const char *A_OCTAVES = "octaves";
constexpr const char *A_WEIGHT = "weight";
constexpr const char *A_PERSISTENCE = "persistence";
constexpr const char *A_LACUNARITY = "lacunarity";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_gabor_wave_fbm_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_CONTROL);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ENVELOPE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ANGLE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUTPUT, CONFIG(node));

  // attribute(s)
  node.set_current_category("Gabor Wave");

  add_wavenumber(node, A_KW, "Spatial Frequency");
  add_angle(node, A_ANGLE, "Angle");
  add_float(node, A_ANGLE_SPREAD_RATIO, "Angle Spread Ratio", 1.f, 0.f, 1.f);
  add_seed(node, A_SEED, "Seed");

  node.set_current_category("FBM Noise");

  add_int(node, A_OCTAVES, "Octaves", 8, 0, 32);
  add_float(node, A_WEIGHT, "Weight", 0.7f, 0.f, 1.f);
  add_float(node, A_PERSISTENCE, "Persistence", 0.5f, 0.f, 1.f);
  add_float(node, A_LACUNARITY, "Lacunarity", 2.f, 0.01f, 4.f);

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_gabor_wave_fbm_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto *p_dx = node.get_value_ref<hmap::VirtualArray>(P_DX);
  auto *p_dy = node.get_value_ref<hmap::VirtualArray>(P_DY);
  auto *p_ctrl = node.get_value_ref<hmap::VirtualArray>(P_CONTROL);
  auto *p_env = node.get_value_ref<hmap::VirtualArray>(P_ENVELOPE);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUTPUT);
  auto *p_angle = node.get_value_ref<hmap::VirtualArray>(P_ANGLE);

  hmap::for_each_tile(
      {p_ctrl, p_dx, p_dy, p_angle},
      {p_out},
      [&node](std::vector<const hmap::Array *> in,
              std::vector<hmap::Array *>       out,
              const hmap::TileRegion          &region)
      {
        auto [pa_ctrl, pa_dx, pa_dy, pa_angle] = unpack<4>(in);
        auto [pa_out] = unpack<1>(out);

        hmap::Array angle_deg(region.shape, node.val<float>(A_ANGLE));

        if (pa_angle)
          angle_deg += (*pa_angle) * 180.f / M_PI;

        *pa_out = hmap::gpu::gabor_wave_fbm(region.shape,
                                            node.val<glm::vec2>(A_KW),
                                            node.val<int>(A_SEED),
                                            angle_deg,
                                            node.val<float>(A_ANGLE_SPREAD_RATIO),
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
