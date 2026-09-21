/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/operator.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_A   = "a";
constexpr const char *P_B   = "b";
constexpr const char *P_OUT = "output";

constexpr const char *A_SLICE_X     = "slide_x";
constexpr const char *A_SLICE_Y     = "slide_y";
constexpr const char *A_SWAP_INPUTS = "swap_inputs";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_compare_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_A);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_B);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  // clang-format off
  add_float(node, A_SLICE_X, "X-cutslice Position", 0.5f, 0.f, 1.f);
  add_float(node, A_SLICE_Y, "Y-cutslice Position", 1.f, 0.f, 1.f);
  add_bool(node, A_SWAP_INPUTS, "Swap Inputs", false);
  // clang-format on

  // --- Attribute(s) order
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_compare_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_a   = node.get_value_ref<hmap::VirtualArray>(P_A);
  auto *p_b   = node.get_value_ref<hmap::VirtualArray>(P_B);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_a || !p_b)
    return;

  // --- Params

  // clang-format off
  const auto sx          = node.val<float>(A_SLICE_X);
  const auto sy          = node.val<float>(A_SLICE_Y);
  const auto swap_inputs = node.val<bool>(A_SWAP_INPUTS);
  // clang-format on

  // --- Adjust inputs

  if (swap_inputs)
    std::swap(p_a, p_b);

  // --- Compute

  hmap::for_each_tile(
      {p_a, p_b},
      {p_out},
      [&](std::vector<const hmap::Array *> in,
          std::vector<hmap::Array *>       out,
          const hmap::TileRegion &)
      {
        auto [pa_a, pa_b] = unpack<2>(in);
        auto [pa_out]     = unpack<1>(out);

        *pa_out = hmap::compare(*pa_a, *pa_b, sx, sy);
      },
      node.cfg().cm_single_array);
}

} // namespace hesiod
