/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
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

constexpr const char *P_ENV = "envelope";
constexpr const char *P_OUT = "output";

constexpr const char *A_SEED    = "seed";
constexpr const char *A_DENSITY = "density";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_white_sparse_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ENV);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  add_seed(node, A_SEED, "Seed");
  add_float(node, A_DENSITY, "Density", 0.1f, 0.f, 1.f);

  // --- Attribute(s) order

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_white_sparse_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_env = node.get_value_ref<hmap::VirtualArray>(P_ENV);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_out)
    return;

  // --- Params

  // clang-format off
  const auto seed    = node.val<int>(A_SEED);
  const auto density = node.val<float>(A_DENSITY);
  // clang-format on

  const float density_per_tile = density / float(p_out->shape.x / p_out->tile_shape.x);

  // --- Compute

  hmap::for_each_tile(
      {p_out},
      [seed, density_per_tile](std::vector<hmap::Array *> p_arrays,
                               const hmap::TileRegion    &region)
      {
        auto [pa_out] = unpack<1>(p_arrays);

        const uint tile_seed = seed + region.key.hash();

        *pa_out = hmap::white_sparse(region.shape, 0.f, 1.f, density_per_tile, tile_seed);
      },
      node.cfg().cm_cpu);

  // --- Post-process

  post_apply_enveloppe(node, *p_out, p_env);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
