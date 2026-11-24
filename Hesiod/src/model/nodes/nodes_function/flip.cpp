/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/transform.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "output";

constexpr const char *A_FLIP_UD   = "flip_ud";
constexpr const char *A_FLIP_LR   = "flip_lr";
constexpr const char *A_TRANSPOSE = "transpose";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_flip_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  node.set_current_category("Flip");
  add_bool(node, A_FLIP_UD, "Flip Up/Down", false);
  add_bool(node, A_FLIP_LR, "Flip Left/Right", false);
  add_bool(node, A_TRANSPOSE, "Transpose", false);

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_flip_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in  = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in)
    return;

  // --- Params

  const bool flip_ud   = node.val<bool>(A_FLIP_UD);
  const bool flip_lr   = node.val<bool>(A_FLIP_LR);
  const bool transpose = node.val<bool>(A_TRANSPOSE);

  // --- Compute

  hmap::for_each_tile(
      {p_in},
      {p_out},
      [flip_ud, flip_lr, transpose](std::vector<const hmap::Array *> p_arrays_in,
                                    std::vector<hmap::Array *>       p_arrays_out,
                                    const hmap::TileRegion &)
      {
        auto [pa_in]  = unpack<1>(p_arrays_in);
        auto [pa_out] = unpack<1>(p_arrays_out);

        *pa_out = *pa_in;

        if (flip_ud)
          hmap::flip_ud(*pa_out);

        if (flip_lr)
          hmap::flip_lr(*pa_out);

        if (transpose)
          *pa_out = hmap::transpose(*pa_out);
      },
      node.cfg().cm_single_array);

  // --- Post-process

  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
