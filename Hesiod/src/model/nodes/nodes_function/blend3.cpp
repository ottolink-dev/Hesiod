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
constexpr const char *P_IN3 = "input 3";
constexpr const char *P_OUT = "output";

constexpr const char *A_METHOD1 = "blending_method1";
constexpr const char *A_K1 = "k1";
constexpr const char *A_RADIUS1 = "radius1";
constexpr const char *A_METHOD2 = "blending_method2";
constexpr const char *A_K2 = "k2";
constexpr const char *A_RADIUS2 = "radius2";
constexpr const char *A_INPUT1_WEIGHT = "input1_weight";
constexpr const char *A_INPUT2_WEIGHT = "input2_weight";
constexpr const char *A_INPUT3_WEIGHT = "input3_weight";
constexpr const char *A_SWAP_12 = "swap_inputs_12";
constexpr const char *A_SWAP_23 = "swap_inputs_23";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_blend3_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN1);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN2);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN3);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  node.set_current_category("Blending 1 & 2");
  add_enum(node,
           A_METHOD1,
           "Method 1",
           enum_mappings.blending_method_map,
           "minimum_smooth");
  add_float(node, A_K1, "k1", 0.1f, 0.01f, 1.f);
  add_float(node, A_RADIUS1, "radius1", 0.05f, 0.f, 0.2f);

  node.set_current_category("Blending 2 & 3");
  add_enum(node,
           A_METHOD2,
           "Method 2",
           enum_mappings.blending_method_map,
           "minimum_smooth");
  add_float(node, A_K2, "k2", 0.1f, 0.01f, 1.f);
  add_float(node, A_RADIUS2, "radius2", 0.05f, 0.f, 0.2f);

  node.set_current_category("Inputs");
  add_float(node, A_INPUT1_WEIGHT, "input1_weight", 1.f, 0.f, 1.f);
  add_float(node, A_INPUT2_WEIGHT, "input2_weight", 1.f, 0.f, 1.f);
  add_float(node, A_INPUT3_WEIGHT, "input3_weight", 1.f, 0.f, 1.f);
  add_bool(node, A_SWAP_12, "swap_inputs_12", false);
  add_bool(node, A_SWAP_23, "swap_inputs_23", false);

  setup_post_process_heightmap_attributes(
      node,
      {.add_mix = false, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_blend3_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in1 = node.get_value_ref<hmap::VirtualArray>(P_IN1);
  auto *p_in2 = node.get_value_ref<hmap::VirtualArray>(P_IN2);
  auto *p_in3 = node.get_value_ref<hmap::VirtualArray>(P_IN3);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in1 || !p_in2 || !p_in3 || !p_out)
    return;

  // --- Params

  const auto swap_12 = node.val<bool>(A_SWAP_12);
  if (swap_12)
    std::swap(p_in1, p_in2);

  const auto swap_23 = node.val<bool>(A_SWAP_23);
  if (swap_23)
    std::swap(p_in2, p_in3);

  const auto k1 = node.val<float>(A_K1);
  const auto radius1 = node.val<float>(A_RADIUS1);
  const auto method1 = node.val<int>(A_METHOD1);
  const auto k2 = node.val<float>(A_K2);
  const auto radius2 = node.val<float>(A_RADIUS2);
  const auto method2 = node.val<int>(A_METHOD2);
  const auto input1_weight = node.val<float>(A_INPUT1_WEIGHT);
  const auto input2_weight = node.val<float>(A_INPUT2_WEIGHT);
  const auto input3_weight = node.val<float>(A_INPUT3_WEIGHT);

  const int ir1 = std::max(1, (int)(radius1 * p_out->shape.x));
  const int ir2 = std::max(1, (int)(radius2 * p_out->shape.x));

  // --- Compute

  // 1 & 2
  blend_heightmaps(node,
                   *p_out,
                   *p_in1,
                   *p_in2,
                   static_cast<BlendingMethod>(method1),
                   k1,
                   ir1,
                   input1_weight,
                   input2_weight);

  // 2 & 3
  blend_heightmaps(node,
                   *p_out,
                   *p_out,
                   *p_in3,
                   static_cast<BlendingMethod>(method2),
                   k2,
                   ir2,
                   1.f,
                   input3_weight);

  // --- Post-process

  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
