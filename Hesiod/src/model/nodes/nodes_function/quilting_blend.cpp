/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/synthesis.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_INPUT_1 = "input 1";
constexpr const char *P_INPUT_2 = "input 2";
constexpr const char *P_INPUT_3 = "input 3";
constexpr const char *P_INPUT_4 = "input 4";
constexpr const char *P_OUT     = "output";

constexpr const char *A_FILTER_WIDTH_RATIO = "filter_width_ratio";
constexpr const char *A_OVERLAP            = "overlap";
constexpr const char *A_PATCH_FLIP         = "patch_flip";
constexpr const char *A_PATCH_ROTATE       = "patch_rotate";
constexpr const char *A_PATCH_TRANSPOSE    = "patch_transpose";
constexpr const char *A_PATCH_WIDTH        = "patch_width";
constexpr const char *A_SEED               = "seed";

void setup_quilting_blend_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_INPUT_1);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_INPUT_2);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_INPUT_3);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_INPUT_4);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_PATCH_WIDTH, "patch_width", 0.3f, 0.1f, 1.f);
  add_float(node, A_OVERLAP, "overlap", 0.9f, 0.05f, 0.95f);
  add_seed(node, A_SEED, "Seed");
  add_bool(node, A_PATCH_FLIP, "patch_flip", true);
  add_bool(node, A_PATCH_ROTATE, "patch_rotate", true);
  add_bool(node, A_PATCH_TRANSPOSE, "patch_transpose", true);
  add_float(node, A_FILTER_WIDTH_RATIO, "filter_width_ratio", 0.5f, 0.f, 1.f);
}

void compute_quilting_blend_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in1 = node.get_value_ref<hmap::VirtualArray>(P_INPUT_1);
  hmap::VirtualArray *p_in2 = node.get_value_ref<hmap::VirtualArray>(P_INPUT_2);
  hmap::VirtualArray *p_in3 = node.get_value_ref<hmap::VirtualArray>(P_INPUT_3);
  hmap::VirtualArray *p_in4 = node.get_value_ref<hmap::VirtualArray>(P_INPUT_4);

  std::vector<hmap::VirtualArray *> ptr_list = {};
  for (auto &ptr : {p_in1, p_in2, p_in3, p_in4})
    if (ptr)
      ptr_list.push_back(ptr);

  if ((int)ptr_list.size())
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    int        ir = std::max(1, (int)(node.val<float>(A_PATCH_WIDTH) * p_out->shape.x));
    glm::ivec2 patch_base_shape = glm::ivec2(ir, ir);

    // --- work on a single array (i.e. not-tiled algo)

    std::vector<hmap::Array>         arrays   = {};
    std::vector<const hmap::Array *> p_arrays = {};

    for (auto &ptr : {p_in1, p_in2, p_in3, p_in4})
      if (ptr)
        arrays.push_back(std::move(ptr->to_array(node.cfg().cm_cpu)));

    for (auto &a : arrays)
      p_arrays.push_back(&a);

    hmap::Array out_array = hmap::quilting_blend(p_arrays,
                                                 patch_base_shape,
                                                 node.val<float>(A_OVERLAP),
                                                 node.val<int>(A_SEED),
                                                 node.val<bool>(A_PATCH_FLIP),
                                                 node.val<bool>(A_PATCH_ROTATE),
                                                 node.val<bool>(A_PATCH_TRANSPOSE),
                                                 node.val<float>(A_FILTER_WIDTH_RATIO));

    p_out->from_array(out_array, node.cfg().cm_cpu);
  }
}

} // namespace hesiod
