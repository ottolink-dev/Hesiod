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

void setup_blend3_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input 1");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input 2");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input 3");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<EnumAttribute>("blending_method1",
                               "blending_method1",
                               enum_mappings.blending_method_map,
                               "minimum_smooth");
  node.add_attr<FloatAttribute>("k1", "k1", 0.1f, 0.01f, 1.f);
  node.add_attr<FloatAttribute>("radius1", "radius1", 0.05f, 0.f, 0.2f);
  node.add_attr<EnumAttribute>("blending_method2",
                               "blending_method2",
                               enum_mappings.blending_method_map,
                               "minimum_smooth");
  node.add_attr<FloatAttribute>("k2", "k2", 0.1f, 0.01f, 1.f);
  node.add_attr<FloatAttribute>("radius2", "radius2", 0.05f, 0.f, 0.2f);
  node.add_attr<FloatAttribute>("input1_weight", "input1_weight", 1.f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("input2_weight", "input2_weight", 1.f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("input3_weight", "input3_weight", 1.f, 0.f, 1.f);
  node.add_attr<BoolAttribute>("swap_inputs_12", "swap_inputs_12", false);
  node.add_attr<BoolAttribute>("swap_inputs_23", "swap_inputs_23", false);

  // attribute(s) order
  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Main Parameters",
                             "_TEXT_Blending 1 & 2",
                             "blending_method1",
                             "k1",
                             "radius1",
                             "_TEXT_Blending 2 & 3",
                             "blending_method2",
                             "k2",
                             "radius2",
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Inputs",
                             "input1_weight",
                             "input2_weight",
                             "input3_weight",
                             "swap_inputs_12",
                             "swap_inputs_23",
                             "_GROUPBOX_END_"});

  setup_post_process_heightmap_attributes(
      node,
      {.add_mix = false, .remap_active_state = false});
}

void compute_blend3_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in1 = node.get_value_ref<hmap::VirtualArray>("input 1");
  hmap::VirtualArray *p_in2 = node.get_value_ref<hmap::VirtualArray>("input 2");
  hmap::VirtualArray *p_in3 = node.get_value_ref<hmap::VirtualArray>("input 3");

  if (p_in1 && p_in2 && p_in3)
  {
    // adjust inputs
    if (node.get_attr<BoolAttribute>("swap_inputs_12"))
      std::swap(p_in1, p_in2);

    if (node.get_attr<BoolAttribute>("swap_inputs_23"))
      std::swap(p_in2, p_in3);

    // compute output
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    // 1 & 2
    {
      float k = node.get_attr<FloatAttribute>("k1");
      int   ir = std::max(1,
                        (int)(node.get_attr<FloatAttribute>("radius1") * p_out->shape.x));
      int   method = node.get_attr<EnumAttribute>("blending_method1");

      blend_heightmaps(node,
                       *p_out,
                       *p_in1,
                       *p_in2,
                       static_cast<BlendingMethod>(method),
                       k,
                       ir,
                       node.get_attr<FloatAttribute>("input1_weight"),
                       node.get_attr<FloatAttribute>("input2_weight"));
    }

    // 2 & 3
    {
      float k = node.get_attr<FloatAttribute>("k2");
      int   ir = std::max(1,
                        (int)(node.get_attr<FloatAttribute>("radius2") * p_out->shape.x));
      int   method = node.get_attr<EnumAttribute>("blending_method2");

      blend_heightmaps(node,
                       *p_out,
                       *p_out,
                       *p_in3,
                       static_cast<BlendingMethod>(method),
                       k,
                       ir,
                       1.f,
                       node.get_attr<FloatAttribute>("input3_weight"));
    }

    // post-process
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
