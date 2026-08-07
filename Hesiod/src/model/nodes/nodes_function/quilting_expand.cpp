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
constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "output";

constexpr const char *A_EXPANSION_RATIO    = "expansion_ratio";
constexpr const char *A_FILTER_WIDTH_RATIO = "filter_width_ratio";
constexpr const char *A_OVERLAP            = "overlap";
constexpr const char *A_PATCH_FLIP         = "patch_flip";
constexpr const char *A_PATCH_ROTATE       = "patch_rotate";
constexpr const char *A_PATCH_TRANSPOSE    = "patch_transpose";
constexpr const char *A_PATCH_WIDTH        = "patch_width";
constexpr const char *A_SEED               = "seed";

void setup_quilting_expand_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_EXPANSION_RATIO, "expansion_ratio", 2.f, 1.f, 16.f);
  add_float(node, A_PATCH_WIDTH, "patch_width", 0.3f, 0.1f, 0.9f);
  add_float(node, A_OVERLAP, "overlap", 0.9f, 0.05f, 0.95f);
  add_seed(node, A_SEED, "Seed");
  add_bool(node, A_PATCH_FLIP, "patch_flip", true);
  add_bool(node, A_PATCH_ROTATE, "patch_rotate", true);
  add_bool(node, A_PATCH_TRANSPOSE, "patch_transpose", true);
  add_float(node, A_FILTER_WIDTH_RATIO, "filter_width_ratio", 0.5f, 0.f, 1.f);
}

void compute_quilting_expand_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    int ir = std::max(1, (int)(node.val<float>(A_PATCH_WIDTH) * p_out->shape.x));

    glm::ivec2 patch_base_shape = glm::ivec2(ir, ir);

    // --- work on a single array (i.e. not-tiled algo)

    hmap::Array in_array  = p_in->to_array(node.cfg().cm_cpu);
    hmap::Array out_array = hmap::Array(p_out->shape);

    out_array = hmap::quilting_expand(in_array,
                                      node.val<float>(A_EXPANSION_RATIO),
                                      patch_base_shape,
                                      node.val<float>(A_OVERLAP),
                                      node.val<int>(A_SEED),
                                      {},   // no secondary arrays
                                      true, // keep_input_shape
                                      node.val<bool>(A_PATCH_FLIP),
                                      node.val<bool>(A_PATCH_ROTATE),
                                      node.val<bool>(A_PATCH_TRANSPOSE),
                                      node.val<float>(A_FILTER_WIDTH_RATIO));

    p_out->from_array(out_array, node.cfg().cm_cpu);
  }
}

} // namespace hesiod
