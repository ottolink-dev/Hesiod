/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/geometry/path.hpp"
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

constexpr const char *P_PATH = "path";
constexpr const char *P_OUT = "out";

constexpr const char *A_SEED = "seed";
constexpr const char *A_ISLAND_COUNT = "island_count";
constexpr const char *A_ISLAND_RADIUS = "island_radius";
constexpr const char *A_SIZE_FALLOFF = "size_falloff";
constexpr const char *A_SIZE_JITTER = "size_jitter";
constexpr const char *A_SCATTER = "scatter";
constexpr const char *A_DISPLACEMENT = "displacement";
constexpr const char *A_NOISE_TYPE = "noise_type";
constexpr const char *A_KW = "kw";
constexpr const char *A_OCTAVES = "octaves";
constexpr const char *A_WEIGHT = "weight";
constexpr const char *A_PERSISTENCE = "persistence";
constexpr const char *A_LACUNARITY = "lacunarity";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_island_chain_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::Path>(gnode::PortType::IN, P_PATH);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  // clang-format off
  node.add_attr<SeedAttribute>(A_SEED, "Seed");
  node.add_attr<IntAttribute>(A_ISLAND_COUNT, "Islands", 5, 1, 64);
  node.add_attr<FloatAttribute>(A_ISLAND_RADIUS, "Radius", 0.1f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_SIZE_FALLOFF, "Size Falloff", 0.5f, -1.f, 1.f);
  node.add_attr<FloatAttribute>(A_SIZE_JITTER, "Size Jitter", 0.3f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_SCATTER, "Scatter", 0.05f, 0.f, 0.5f);
  node.add_attr<FloatAttribute>(A_DISPLACEMENT, "Displacement", 0.02f, 0.f, 0.5f);
  node.add_attr<EnumAttribute>(A_NOISE_TYPE, "Type", enum_mappings.noise_type_map);
  node.add_attr<FloatAttribute>(A_KW, "kw", 4.f, 0.f, FLT_MAX);
  node.add_attr<IntAttribute>(A_OCTAVES, "Octaves", 8, 0, 32);
  node.add_attr<FloatAttribute>(A_WEIGHT, "Weight", 0.7f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_PERSISTENCE, "Persistence", 0.5f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_LACUNARITY, "Lacunarity", 2.f, 0.01f, 4.f);
  // clang-format on

  // --- Attribute(s) order

  node.set_attr_ordered_key({
      "_GROUPBOX_BEGIN_Islands",
      A_SEED,
      A_ISLAND_COUNT,
      A_ISLAND_RADIUS,
      A_SIZE_FALLOFF,
      A_SIZE_JITTER,
      A_SCATTER,
      A_DISPLACEMENT,
      "_GROUPBOX_END_",
      //
      "_GROUPBOX_BEGIN_Noise",
      A_NOISE_TYPE,
      A_KW,
      A_OCTAVES,
      A_WEIGHT,
      A_PERSISTENCE,
      A_LACUNARITY,
      "_GROUPBOX_END_",
  });

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_island_chain_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_path = node.get_value_ref<hmap::Path>(P_PATH);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_out || !p_path || p_path->size() <= 1)
    return;

  // --- Params

  // clang-format off
  const auto seed          = node.get_attr<SeedAttribute>(A_SEED);
  const auto island_count  = node.get_attr<IntAttribute>(A_ISLAND_COUNT);
  const auto island_radius = node.get_attr<FloatAttribute>(A_ISLAND_RADIUS);
  const auto size_falloff  = node.get_attr<FloatAttribute>(A_SIZE_FALLOFF);
  const auto size_jitter   = node.get_attr<FloatAttribute>(A_SIZE_JITTER);
  const auto scatter       = node.get_attr<FloatAttribute>(A_SCATTER);
  const auto displacement  = node.get_attr<FloatAttribute>(A_DISPLACEMENT);
  const auto noise_type    = hmap::NoiseType(node.get_attr<EnumAttribute>(A_NOISE_TYPE));
  const auto kw            = node.get_attr<FloatAttribute>(A_KW);
  const auto octaves       = node.get_attr<IntAttribute>(A_OCTAVES);
  const auto weight        = node.get_attr<FloatAttribute>(A_WEIGHT);
  const auto persistence   = node.get_attr<FloatAttribute>(A_PERSISTENCE);
  const auto lacunarity    = node.get_attr<FloatAttribute>(A_LACUNARITY);
  // clang-format on

  // --- Compute

  hmap::for_each_tile(
      {p_out},
      [&](std::vector<hmap::Array *> out, const hmap::TileRegion &region)
      {
        auto [pa_out] = unpack<1>(out);

        *pa_out = hmap::island_chain_land_mask(region.shape,
                                               *p_path,
                                               seed,
                                               island_count,
                                               island_radius,
                                               size_falloff,
                                               size_jitter,
                                               scatter,
                                               displacement,
                                               noise_type,
                                               kw,
                                               octaves,
                                               weight,
                                               persistence,
                                               lacunarity,
                                               region.bbox);
      },
      node.cfg().cm_cpu);

  // --- Post-process

  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
