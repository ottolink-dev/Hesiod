/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/synthesis.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_quilting_expand_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("expansion_ratio", "expansion_ratio", 2.f, 1.f, 16.f);
  node.add_attr<FloatAttribute>("patch_width", "patch_width", 0.3f, 0.1f, 0.9f);
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
  node.set_attr_ordered_key({"expansion_ratio",
                             "patch_width",
                             "overlap",
                             "seed",
                             "patch_flip",
                             "patch_rotate",
                             "patch_transpose",
                             "filter_width_ratio"});
}

void compute_quilting_expand_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    int ir = std::max(
        1,
        (int)(node.get_attr<FloatAttribute>("patch_width") * p_out->shape.x));

    glm::ivec2 patch_base_shape = glm::ivec2(ir, ir);

    // --- work on a single array (i.e. not-tiled algo)

    hmap::Array in_array = p_in->to_array(node.cfg().cm_cpu);
    hmap::Array out_array = hmap::Array(p_out->shape);

    out_array = hmap::quilting_expand(
        in_array,
        node.get_attr<FloatAttribute>("expansion_ratio"),
        patch_base_shape,
        node.get_attr<FloatAttribute>("overlap"),
        node.get_attr<SeedAttribute>("seed"),
        {},   // no secondary arrays
        true, // keep_input_shape
        node.get_attr<BoolAttribute>("patch_flip"),
        node.get_attr<BoolAttribute>("patch_rotate"),
        node.get_attr<BoolAttribute>("patch_transpose"),
        node.get_attr<FloatAttribute>("filter_width_ratio"));

    p_out->from_array(out_array, node.cfg().cm_cpu);
  }
}

} // namespace hesiod
