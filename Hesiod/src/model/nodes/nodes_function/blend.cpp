/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/blending.hpp"
#include "highmap/range.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN1 = "input 1";
constexpr const char *P_IN2 = "input 2";
constexpr const char *P_OUT = "output";

constexpr const char *A_METHOD        = "blending_method";
constexpr const char *A_K             = "k";
constexpr const char *A_RADIUS        = "radius";
constexpr const char *A_INPUT1_WEIGHT = "input1_weight";
constexpr const char *A_INPUT2_WEIGHT = "input2_weight";
constexpr const char *A_SWAP_INPUTS   = "swap_inputs";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_blend_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN1);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN2);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  node.set_current_category("Main Parameters");
  add_enum(node, A_METHOD, "Method:", enum_mappings.blending_method_map, "minimum_smooth");
  add_float(node, A_K, "k", 0.1f, 0.01f, 1.f);
  add_float(node, A_RADIUS, "radius", 0.05f, 0.f, 0.2f);

  node.set_current_category("Inputs");
  add_float(node, A_INPUT1_WEIGHT, "input1_weight", 1.f, 0.f, 1.f);
  add_float(node, A_INPUT2_WEIGHT, "input2_weight", 1.f, 0.f, 1.f);
  add_bool(node, A_SWAP_INPUTS, "swap_inputs", false);

  setup_post_process_heightmap_attributes(
      node,
      {.add_mix = false, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_blend_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in1 = node.get_value_ref<hmap::VirtualArray>(P_IN1);
  auto *p_in2 = node.get_value_ref<hmap::VirtualArray>(P_IN2);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in1 || !p_in2 || !p_out)
    return;

  // --- Params

  const auto swap = node.val<bool>(A_SWAP_INPUTS);
  if (swap)
    std::swap(p_in1, p_in2);

  const auto k             = node.val<float>(A_K);
  const auto radius        = node.val<float>(A_RADIUS);
  const auto method        = node.val<int>(A_METHOD);
  const auto input1_weight = node.val<float>(A_INPUT1_WEIGHT);
  const auto input2_weight = node.val<float>(A_INPUT2_WEIGHT);

  const int ir = std::max(1, (int)(radius * p_out->shape.x));

  // --- Compute

  blend_heightmaps(node,
                   *p_out,
                   *p_in1,
                   *p_in2,
                   static_cast<BlendingMethod>(method),
                   k,
                   ir,
                   input1_weight,
                   input2_weight);

  // --- Post-process

  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
