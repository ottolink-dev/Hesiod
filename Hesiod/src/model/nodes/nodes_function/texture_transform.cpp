/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/texture.hpp"
#include "highmap/transform.hpp"
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
constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "output";

constexpr const char *A_FLIP_X    = "flip_x";
constexpr const char *A_FLIP_Y    = "flip_y";
constexpr const char *A_TRANSPOSE = "transpose";

void setup_texture_transform_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // ports
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT, P_OUT, CONFIG_TEX(node));

  // attributes
  add_bool(node, A_FLIP_X, "Flip-X", false);
  add_bool(node, A_FLIP_Y, "Flip-Y", false);
  add_bool(node, A_TRANSPOSE, "Transpose", false);
}

void compute_texture_transform_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto *p_in  = node.get_value_ref<hmap::VirtualTexture>(P_IN);
  auto *p_out = node.get_value_ref<hmap::VirtualTexture>(P_OUT);

  if (p_in && p_out)
  {
    const bool flip_x    = node.val<bool>(A_FLIP_X);
    const bool flip_y    = node.val<bool>(A_FLIP_Y);
    const bool transpose = node.val<bool>(A_TRANSPOSE);

    hmap::Texture t = p_in->to_texture(p_in->shape, node.cfg().cm_cpu);

    if (flip_x)
      hmap::flip_lr(t);
    if (flip_y)
      hmap::flip_ud(t);
    if (transpose)
      t = hmap::transpose(t);

    hmap::VirtualTexture temp_tex(t.shape,
                                  node.cfg().tile_shape,
                                  node.cfg().halo,
                                  t.num_channels(),
                                  node.cfg().storage_mode);

    std::vector<const hmap::Array *> arrays;
    for (int c = 0; c < t.num_channels(); ++c)
      arrays.push_back(&t.channels[c]);

    temp_tex.from_arrays(arrays, node.cfg().cm_cpu);

    p_out->copy_from(temp_tex, node.cfg().cm_cpu);
  }
}

} // namespace hesiod
