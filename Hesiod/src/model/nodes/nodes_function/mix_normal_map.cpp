/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/colorize.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_mix_normal_map_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, "normal map base");
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, "normal map detail");
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT,
                                      "normal map",
                                      CONFIG_TEX(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("detail_scaling", "detail_scaling", 1.f, 0.f, 4.f);
  node.add_attr<EnumAttribute>("blending_method",
                               "blending_method",
                               hmap::normal_map_blending_method_as_string);

  // attribute(s) order
  node.set_attr_ordered_key({"detail_scaling", "blending_method"});
}

void compute_mix_normal_map_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualTexture *p_in1 = node.get_value_ref<hmap::VirtualTexture>(
      "normal map base");
  hmap::VirtualTexture *p_in2 = node.get_value_ref<hmap::VirtualTexture>(
      "normal map detail");

  if (p_in1 && p_in2)
  {
    hmap::VirtualTexture *p_out = node.get_value_ref<hmap::VirtualTexture>("normal map");

    hmap::mix_normal_map(
        *p_out,
        *p_in1,
        *p_in2,
        node.cfg().cm_cpu,
        node.get_attr<FloatAttribute>("detail_scaling"),
        (hmap::NormalMapBlendingMethod)node.get_attr<EnumAttribute>("blending_method"));
  }
}

} // namespace hesiod
