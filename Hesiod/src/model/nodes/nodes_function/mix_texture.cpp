/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/colorize.hpp"
#include "highmap/kernels.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_TEXTURE  = "texture";
constexpr const char *P_TEXTURE1 = "texture1";
constexpr const char *P_TEXTURE2 = "texture2";
constexpr const char *P_TEXTURE3 = "texture3";
constexpr const char *P_TEXTURE4 = "texture4";

constexpr const char *A_RESET_OUTPUT_ALPHA = "reset_output_alpha";
constexpr const char *A_USE_SQRT_AVG       = "use_sqrt_avg";

void setup_mix_texture_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_TEXTURE1);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_TEXTURE2);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_TEXTURE3);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_TEXTURE4);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT, P_TEXTURE, CONFIG_TEX(node));

  // attribute(s)
  add_bool(node, A_USE_SQRT_AVG, "use_sqrt_avg", true);
  add_bool(node, A_RESET_OUTPUT_ALPHA, "reset_output_alpha", true);
}

void compute_mix_texture_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualTexture *p_in1 = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE1);
  hmap::VirtualTexture *p_in2 = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE2);
  hmap::VirtualTexture *p_in3 = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE3);
  hmap::VirtualTexture *p_in4 = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE4);
  hmap::VirtualTexture *p_out = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE);

  std::vector<hmap::VirtualTexture *> ptr_list = {};

  for (auto &ptr : {p_in1, p_in2, p_in3, p_in4})
    if (ptr)
      ptr_list.push_back(ptr);

  if ((int)ptr_list.size())
  {
    mix(*p_out, ptr_list, node.cfg().cm_cpu, node.val<bool>(A_USE_SQRT_AVG));

    if (node.val<bool>(A_RESET_OUTPUT_ALPHA))
      p_out->fill(3, 1.f, node.cfg().cm_cpu);
  }
}

} // namespace hesiod
