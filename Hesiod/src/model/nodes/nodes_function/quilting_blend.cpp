/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/synthesis.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_quilting_blend_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input 1");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input 2");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input 3");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input 4");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("patch_width", "patch_width", 0.3f, 0.1f, 1.f);
  node.add_attr<FloatAttribute>("overlap", "overlap", 0.9f, 0.05f, 0.95f);
  node.add_attr<SeedAttribute>("seed", "Seed");
  node.add_attr<BoolAttribute>("patch_flip", "patch_flip", true);
  node.add_attr<BoolAttribute>("patch_rotate", "patch_rotate", true);
  node.add_attr<BoolAttribute>("patch_transpose", "patch_transpose", true);
  node.add_attr<FloatAttribute>("filter_width_ratio",
                                "filter_width_ratio",
                                0.5f,
                                0.f,
                                1.f);

  // attribute(s) order
  node.set_attr_ordered_key({"patch_width",
                             "overlap",
                             "seed",
                             "patch_flip",
                             "patch_rotate",
                             "patch_transpose",
                             "filter_width_ratio"});
}

void compute_quilting_blend_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in1 = node.get_value_ref<hmap::VirtualArray>("input 1");
  hmap::VirtualArray *p_in2 = node.get_value_ref<hmap::VirtualArray>("input 2");
  hmap::VirtualArray *p_in3 = node.get_value_ref<hmap::VirtualArray>("input 3");
  hmap::VirtualArray *p_in4 = node.get_value_ref<hmap::VirtualArray>("input 4");

  std::vector<hmap::VirtualArray *> ptr_list = {};
  for (auto &ptr : {p_in1, p_in2, p_in3, p_in4})
    if (ptr)
      ptr_list.push_back(ptr);

  if ((int)ptr_list.size())
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    int ir = std::max(
        1,
        (int)(node.get_attr<FloatAttribute>("patch_width") * p_out->shape.x));
    glm::ivec2 patch_base_shape = glm::ivec2(ir, ir);

    // --- work on a single array (i.e. not-tiled algo)

    std::vector<hmap::Array>         arrays   = {};
    std::vector<const hmap::Array *> p_arrays = {};

    for (auto &ptr : {p_in1, p_in2, p_in3, p_in4})
      if (ptr)
        arrays.push_back(std::move(ptr->to_array(node.cfg().cm_cpu)));

    for (auto &a : arrays)
      p_arrays.push_back(&a);

    hmap::Array out_array = hmap::quilting_blend(
        p_arrays,
        patch_base_shape,
        node.get_attr<FloatAttribute>("overlap"),
        node.get_attr<SeedAttribute>("seed"),
        node.get_attr<BoolAttribute>("patch_flip"),
        node.get_attr<BoolAttribute>("patch_rotate"),
        node.get_attr<BoolAttribute>("patch_transpose"),
        node.get_attr<FloatAttribute>("filter_width_ratio"));

    p_out->from_array(out_array, node.cfg().cm_cpu);
  }
}

} // namespace hesiod
