/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN = "input";
constexpr const char *P_SMASK = "seed_mask";
constexpr const char *P_OUT = "output";

constexpr const char *A_SLOPE = "slope";
constexpr const char *A_NOISE_RATIO = "nosie_ratio";
constexpr const char *A_SEED = "seed";
constexpr const char *A_IR = "ir";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_fill_talus_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_SMASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>(A_SLOPE, "slope", 4.f, 0.1f, FLT_MAX);
  node.add_attr<FloatAttribute>(A_NOISE_RATIO, "noise_ratio", 0.2f, 0.f, 1.f);
  node.add_attr<SeedAttribute>(A_SEED, "Seed");
  node.add_attr<IntAttribute>(A_IR, "Radius Search", 1, 1, 8);

  // attribute(s) order
  node.set_attr_ordered_key({A_SLOPE, A_NOISE_RATIO, A_SEED, A_IR});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_fill_talus_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_seed_mask = node.get_value_ref<hmap::VirtualArray>(P_SMASK);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in)
    return;

  // --- Params

  // clang-format off
  const auto seed        = node.get_attr<SeedAttribute>(A_SEED);
  const auto ir          = node.get_attr<IntAttribute>(A_IR);
  const auto noise_ratio = node.get_attr<FloatAttribute>(A_NOISE_RATIO);
  // clang-format on

  float talus = node.get_attr<FloatAttribute>(A_SLOPE) / (float)p_out->shape.x;

  // --- Compute

  hmap::for_each_tile(
      {p_out, p_in, p_seed_mask},
      [&](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
      {
        auto [pa_out, pa_in, pa_seed_mask] = unpack<3>(p_arrays);
        *pa_out = *pa_in;

        hmap::fill_talus(*pa_out, talus, seed, ir, noise_ratio, pa_seed_mask);
      },
      node.cfg().cm_cpu);

  p_out->smooth_overlap_buffers();
}

} // namespace hesiod
