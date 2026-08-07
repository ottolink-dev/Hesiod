/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/colorize.hpp"
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
constexpr const char *P_NORMAL_MAP        = "normal map";
constexpr const char *P_NORMAL_MAP_BASE   = "normal map base";
constexpr const char *P_NORMAL_MAP_DETAIL = "normal map detail";

constexpr const char *A_BLENDING_METHOD = "blending_method";
constexpr const char *A_DETAIL_SCALING  = "detail_scaling";

void setup_mix_normal_map_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_NORMAL_MAP_BASE);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_NORMAL_MAP_DETAIL);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT,
                                      P_NORMAL_MAP,
                                      CONFIG_TEX(node));

  // attribute(s)
  add_float(node, A_DETAIL_SCALING, "detail_scaling", 1.f, 0.f, 4.f);
  add_enum(node,
           A_BLENDING_METHOD,
           "blending_method",
           hmap::normal_map_blending_method_as_string);
}

void compute_mix_normal_map_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualTexture *p_in1 = node.get_value_ref<hmap::VirtualTexture>(
      P_NORMAL_MAP_BASE);
  hmap::VirtualTexture *p_in2 = node.get_value_ref<hmap::VirtualTexture>(
      P_NORMAL_MAP_DETAIL);

  if (p_in1 && p_in2)
  {
    hmap::VirtualTexture *p_out = node.get_value_ref<hmap::VirtualTexture>(P_NORMAL_MAP);

    hmap::mix_normal_map(*p_out,
                         *p_in1,
                         *p_in2,
                         node.cfg().cm_cpu,
                         node.val<float>(A_DETAIL_SCALING),
                         (hmap::NormalMapBlendingMethod)node.val<int>(A_BLENDING_METHOD));
  }
}

} // namespace hesiod
