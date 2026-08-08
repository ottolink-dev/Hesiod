/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_ALPHA   = "alpha";
constexpr const char *P_TEXTURE = "texture";

constexpr const char *A_COLOR = "color";
constexpr const char *A_ALPHA = "alpha";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_colorize_solid_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ALPHA);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT, P_TEXTURE, CONFIG_TEX(node));

  // --- Attributes

  add_color(node, A_COLOR, "Solid Color", {0.f, 1.f, 0.f, 1.f});
  add_float(node, A_ALPHA, "Transparency", 1.f, 0.f, 1.f);
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_colorize_solid_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_alpha   = node.get_value_ref<hmap::VirtualArray>(P_ALPHA);
  auto *p_texture = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE);

  // --- Params

  const auto color = node.val<glm::vec4>(A_COLOR);
  const auto alpha = node.val<float>(A_ALPHA);

  // --- Compute

  for (int nch = 0; nch < 4; ++nch)
  {
    if (nch < 3)
    {
      p_texture->fill(nch, color[nch], node.cfg().cm_cpu);
    }
    else
    {
      // use alpha input port if available
      if (p_alpha)
        hmap::copy_data(*p_alpha, p_texture->channel(nch), node.cfg().cm_cpu);
      else
        p_texture->fill(nch, alpha, node.cfg().cm_cpu);
    }
  }
}

} // namespace hesiod
