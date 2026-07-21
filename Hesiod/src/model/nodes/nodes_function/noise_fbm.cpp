/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/primitives.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
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
constexpr const char *P_CTRL = "control";
constexpr const char *P_ENV = "envelope";
constexpr const char *P_OUT = "output";

constexpr const char *A_NOISE_TYPE = "noise_type";
constexpr const char *A_KW = "kw";
constexpr const char *A_SEED = "seed";
constexpr const char *A_OCTAVES = "octaves";
constexpr const char *A_WEIGHT = "weight";
constexpr const char *A_PERSISTENCE = "persistence";
constexpr const char *A_LACUNARITY = "lacunarity";
constexpr const char *A_PERIODIC = "periodic";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_noise_fbm_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_CTRL);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ENV);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  // clang-format off
  node.add_attr<EnumAttribute>(A_NOISE_TYPE, "Type", enum_mappings.noise_type_map_fbm);
  node.add_attr<WaveNbAttribute>(A_KW, "Spatial Frequency");
  node.add_attr<SeedAttribute>(A_SEED, "Seed");
  node.add_attr<IntAttribute>(A_OCTAVES, "Octaves", 8, 0, 32);
  node.add_attr<FloatAttribute>(A_WEIGHT, "Weight", 0.7f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_PERSISTENCE, "Persistence", 0.5f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_LACUNARITY, "Lacunarity", 2.f, 0.01f, 4.f);
  node.add_attr<BoolAttribute>(A_PERIODIC, "Periodic (tileable)", false);
  // clang-format on

  // --- Attribute(s) order

  node.set_attr_ordered_key({
      "_GROUPBOX_BEGIN_Main Parameters",
      A_NOISE_TYPE,
      A_KW,
      A_SEED,
      A_OCTAVES,
      "_GROUPBOX_END_",
      //
      "_GROUPBOX_BEGIN_fBm layers",
      A_WEIGHT,
      A_PERSISTENCE,
      A_LACUNARITY,
      "_GROUPBOX_END_",
      //
      "_GROUPBOX_BEGIN_Tiling",
      A_PERIODIC,
      "_GROUPBOX_END_",
  });

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_noise_fbm_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_dx = node.get_value_ref<hmap::VirtualArray>(P_DX);
  auto *p_dy = node.get_value_ref<hmap::VirtualArray>(P_DY);
  auto *p_ctrl = node.get_value_ref<hmap::VirtualArray>(P_CTRL);
  auto *p_env = node.get_value_ref<hmap::VirtualArray>(P_ENV);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_out)
    return;

  // --- Params

  // clang-format off
  const auto noise_type  = hmap::NoiseType(node.get_attr<EnumAttribute>(A_NOISE_TYPE));
  const auto kw          = node.get_attr<WaveNbAttribute>(A_KW);
  const auto seed        = node.get_attr<SeedAttribute>(A_SEED);
  const auto octaves     = node.get_attr<IntAttribute>(A_OCTAVES);
  const auto weight      = node.get_attr<FloatAttribute>(A_WEIGHT);
  const auto persistence = node.get_attr<FloatAttribute>(A_PERSISTENCE);
  const auto lacunarity  = node.get_attr<FloatAttribute>(A_LACUNARITY);
  const auto periodic    = node.get_attr<BoolAttribute>(A_PERIODIC);
  // clang-format on

  // --- Compute

  hmap::for_each_tile(
      {p_dx, p_dy, p_ctrl},
      {p_out},
      [&](std::vector<const hmap::Array *> in,
          std::vector<hmap::Array *>       out,
          const hmap::TileRegion          &region)
      {
        auto [pa_dx, pa_dy, pa_ctrl] = unpack<3>(in);
        auto [pa_out] = unpack<1>(out);

        // When periodic, snap kw to integer cells so the lattice wrap
        // aligns with the noise frequency and the result tiles
        // seamlessly.

        glm::vec2  kw_local = kw;
        glm::ivec2 period(0, 0);

        if (periodic)
        {
          kw_local = glm::vec2(float(int(kw_local.x + 0.5f)),
                               float(int(kw_local.y + 0.5f)));

          period = glm::ivec2(int(kw_local.x), int(kw_local.y));
        }

        *pa_out = hmap::gpu::noise_fbm(noise_type,
                                       region.shape,
                                       kw_local,
                                       seed,
                                       octaves,
                                       weight,
                                       persistence,
                                       lacunarity,
                                       pa_ctrl,
                                       pa_dx,
                                       pa_dy,
                                       nullptr,
                                       region.bbox,
                                       period);
      },
      node.cfg().cm_gpu);

  // --- Post-process

  post_apply_enveloppe(node, *p_out, p_env);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
