/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/range.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_combine_mask_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input 1");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input 2");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<EnumAttribute>("method",
                               "method",
                               enum_mappings.mask_combine_method_map,
                               "intersection");
  node.add_attr<BoolAttribute>("swap_inputs", "swap_inputs", false);

  // attribute(s) order
  node.set_attr_ordered_key(
      {"_GROUPBOX_BEGIN_Main Parameters", "method", "swap_inputs", "_GROUPBOX_END_"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_combine_mask_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in1 = node.get_value_ref<hmap::VirtualArray>("input 1");
  hmap::VirtualArray *p_in2 = node.get_value_ref<hmap::VirtualArray>("input 2");

  if (!p_in1 || !p_in2)
    return;

  // adjust inputs
  if (node.get_attr<BoolAttribute>("swap_inputs"))
    std::swap(p_in1, p_in2);

  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

  std::function<void(std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)>
      lambda;

  int method = node.get_attr<EnumAttribute>("method");

  switch (method)
  {
  case MaskCombineMethod::UNION:
    lambda = [](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
    {
      hmap::Array &m = *p_arrays[0];
      hmap::Array &a1 = *p_arrays[1];
      hmap::Array &a2 = *p_arrays[2];
      m = hmap::maximum(a1, a2);
    };
    break;

  case MaskCombineMethod::INTERSECTION:
    lambda = [](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
    {
      hmap::Array &m = *p_arrays[0];
      hmap::Array &a1 = *p_arrays[1];
      hmap::Array &a2 = *p_arrays[2];
      m = hmap::minimum(a1, a2);
    };
    break;

  case MaskCombineMethod::EXCLUSION:
    lambda = [](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
    {
      hmap::Array &m = *p_arrays[0];
      hmap::Array &a1 = *p_arrays[1];
      hmap::Array &a2 = *p_arrays[2];
      m = a1 - a2;
      hmap::clamp_min(m, 0.f);
    };
    break;
  }

  hmap::for_each_tile({p_out, p_in1, p_in2}, lambda, node.cfg().cm_cpu);

  // post-process
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
