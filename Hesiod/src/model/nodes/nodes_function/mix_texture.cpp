/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/colorize.hpp"
#include "highmap/kernels.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_mix_texture_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, "texture1");
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, "texture2");
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, "texture3");
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, "texture4");
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT, "texture", CONFIG_TEX(node));

  // attribute(s)
  node.add_attr<BoolAttribute>("use_sqrt_avg", "use_sqrt_avg", true);
  node.add_attr<BoolAttribute>("reset_output_alpha", "reset_output_alpha", true);

  // attribute(s) order
  node.set_attr_ordered_key({"use_sqrt_avg", "reset_output_alpha"});
}

void compute_mix_texture_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualTexture *p_in1 = node.get_value_ref<hmap::VirtualTexture>("texture1");
  hmap::VirtualTexture *p_in2 = node.get_value_ref<hmap::VirtualTexture>("texture2");
  hmap::VirtualTexture *p_in3 = node.get_value_ref<hmap::VirtualTexture>("texture3");
  hmap::VirtualTexture *p_in4 = node.get_value_ref<hmap::VirtualTexture>("texture4");
  hmap::VirtualTexture *p_out = node.get_value_ref<hmap::VirtualTexture>("texture");

  std::vector<hmap::VirtualTexture *> ptr_list = {};

  for (auto &ptr : {p_in1, p_in2, p_in3, p_in4})
    if (ptr)
      ptr_list.push_back(ptr);

  if ((int)ptr_list.size())
  {
    mix(*p_out,
        ptr_list,
        node.cfg().cm_cpu,
        node.get_attr<BoolAttribute>("use_sqrt_avg"));

    if (node.get_attr<BoolAttribute>("reset_output_alpha"))
      p_out->fill(3, 1.f, node.cfg().cm_cpu);
  }
}

} // namespace hesiod
