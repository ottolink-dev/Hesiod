/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/blending.hpp"
#include "highmap/range.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_blend_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input 1");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input 2");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<EnumAttribute>("blending_method",
                               "Method:",
                               enum_mappings.blending_method_map,
                               "minimum_smooth");
  node.add_attr<FloatAttribute>("k", "k", 0.1f, 0.01f, 1.f);
  node.add_attr<FloatAttribute>("radius", "radius", 0.05f, 0.f, 0.2f);
  node.add_attr<FloatAttribute>("input1_weight", "input1_weight", 1.f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("input2_weight", "input2_weight", 1.f, 0.f, 1.f);
  node.add_attr<BoolAttribute>("swap_inputs", "swap_inputs", false);

  // attribute(s) order
  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Main Parameters",
                             "blending_method",
                             "k",
                             "radius",
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Inputs",
                             "input1_weight",
                             "input2_weight",
                             "swap_inputs",
                             "_GROUPBOX_END_"});

  setup_post_process_heightmap_attributes(
      node,
      {.add_mix = false, .remap_active_state = false});
}

void compute_blend_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in1 = node.get_value_ref<hmap::VirtualArray>("input 1");
  hmap::VirtualArray *p_in2 = node.get_value_ref<hmap::VirtualArray>("input 2");

  if (p_in1 && p_in2)
  {
    // adjust inputs
    if (node.get_attr<BoolAttribute>("swap_inputs"))
      std::swap(p_in1, p_in2);

    // compute output
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    float k = node.get_attr<FloatAttribute>("k");
    int ir = std::max(1, (int)(node.get_attr<FloatAttribute>("radius") * p_out->shape.x));
    int method = node.get_attr<EnumAttribute>("blending_method");

    blend_heightmaps(node,
                     *p_out,
                     *p_in1,
                     *p_in2,
                     static_cast<BlendingMethod>(method),
                     k,
                     ir,
                     node.get_attr<FloatAttribute>("input1_weight"),
                     node.get_attr<FloatAttribute>("input2_weight"));

    // post-process
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
