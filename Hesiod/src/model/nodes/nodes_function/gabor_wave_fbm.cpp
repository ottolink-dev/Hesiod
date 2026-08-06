/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/primitives.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

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

  node.add_attr<WaveNbAttribute>(A_KW, "Spatial Frequency");
  node.add_attr<FloatAttribute>(A_ANGLE, "Angle", 0.f, -180.f, 180.f, "{:.1f}°");
  node.add_attr<FloatAttribute>(A_ANGLE_SPREAD_RATIO,
                                "Angle Spread Ratio",
                                1.f,
                                0.f,
                                1.f);
  node.add_attr<SeedAttribute>(A_SEED, "Seed");

  node.set_current_category("FBM Noise");

  node.add_attr<IntAttribute>(A_OCTAVES, "Octaves", 8, 0, 32);
  node.add_attr<FloatAttribute>(A_WEIGHT, "Weight", 0.7f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_PERSISTENCE, "Persistence", 0.5f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_LACUNARITY, "Lacunarity", 2.f, 0.01f, 4.f);

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
      {p_out, p_ctrl, p_dx, p_dy, p_angle},
      [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
      {
        hmap::Array *pa_out = p_arrays[0];
        hmap::Array *pa_ctrl = p_arrays[1];
        hmap::Array *pa_dx = p_arrays[2];
        hmap::Array *pa_dy = p_arrays[3];
        hmap::Array *pa_angle = p_arrays[4];

        hmap::Array angle_deg(region.shape, node.get_attr<FloatAttribute>(A_ANGLE));

        if (pa_angle)
          angle_deg += (*pa_angle) * 180.f / M_PI;

        *pa_out = hmap::gpu::gabor_wave_fbm(
            region.shape,
            node.get_attr<WaveNbAttribute>(A_KW),
            node.get_attr<SeedAttribute>(A_SEED),
            angle_deg,
            node.get_attr<FloatAttribute>(A_ANGLE_SPREAD_RATIO),
            node.get_attr<IntAttribute>(A_OCTAVES),
            node.get_attr<FloatAttribute>(A_WEIGHT),
            node.get_attr<FloatAttribute>(A_PERSISTENCE),
            node.get_attr<FloatAttribute>(A_LACUNARITY),
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
