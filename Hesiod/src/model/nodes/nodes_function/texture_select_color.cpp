/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/colorize.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_TEX = "texture";
constexpr const char *P_MASK = "mask";

constexpr const char *A_COLOR = "color";
constexpr const char *A_TOLERANCE = "tolerance";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_texture_select_color_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_TEX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_MASK, CONFIG(node));

  // --- Attributes

  // clang-format off
  node.add_attr<ColorAttribute>(A_COLOR, "Color", 0.f, 0.8f, 0.4f, 1.f);
  node.add_attr<FloatAttribute>(A_TOLERANCE, "Tolerance", 0.1f, 0.f, 0.5f);
  // clang-format off

    // --- Attribute(s) order

  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Color Selection",
                             A_COLOR,
                             A_TOLERANCE,
                             "_GROUPBOX_END_"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}
  
// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_texture_select_color_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_tex = node.get_value_ref<hmap::VirtualTexture>(P_TEX);
  auto *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);

  if (!p_tex)
    return;
    
  // --- Params

  // clang-format off
  const auto color_v   = node.get_attr<ColorAttribute>(A_COLOR);
  const auto tolerance = node.get_attr<FloatAttribute>(A_TOLERANCE);
  // clang-format on

  glm::vec3 color = {color_v[0], color_v[1], color_v[2]};

  // --- Compute mode

  hmap::for_each_tile(
      {&p_tex->channel(0), &p_tex->channel(1), &p_tex->channel(2)},
      {p_mask},
      [&](std::vector<const hmap::Array *> in,
          std::vector<hmap::Array *>       out,
          const hmap::TileRegion &)
      {
        auto [pa_r, pa_g, pa_b] = unpack<3>(in);
        auto [pa_mask] = unpack<1>(out);

        *pa_mask = hmap::color_match_mask(*pa_r, *pa_g, *pa_b, color, tolerance);
      },
      node.cfg().cm_cpu);

  // --- Post-process

  post_process_heightmap(node, *p_mask);
}

} // namespace hesiod
