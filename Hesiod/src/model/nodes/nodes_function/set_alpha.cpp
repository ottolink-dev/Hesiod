/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/range.hpp"
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
constexpr const char *P_ALPHA       = "alpha";
constexpr const char *P_NOISE       = "noise";
constexpr const char *P_TEXTURE_IN  = "texture in";
constexpr const char *P_TEXTURE_OUT = "texture out";

constexpr const char *A_ALPHA   = "alpha";
constexpr const char *A_CLAMP   = "clamp";
constexpr const char *A_REVERSE = "reverse";

void setup_set_alpha_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_TEXTURE_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ALPHA);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_NOISE);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT,
                                      P_TEXTURE_OUT,
                                      CONFIG_TEX(node));

  // attribute(s)
  add_float(node, A_ALPHA, "alpha", 1.f, 0.f, 1.f);
  add_bool(node, A_REVERSE, "reverse", false);
  add_bool(node, A_CLAMP, "clamp", true);
}

void compute_set_alpha_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualTexture *p_in = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE_IN);

  if (p_in)
  {
    hmap::VirtualArray   *p_alpha = node.get_value_ref<hmap::VirtualArray>(P_ALPHA);
    hmap::VirtualArray   *p_noise = node.get_value_ref<hmap::VirtualArray>(P_NOISE);
    hmap::VirtualTexture *p_tex = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE_OUT);

    p_tex->copy_from(*p_in, node.cfg().cm_cpu);

    if (p_alpha)
    {
      hmap::VirtualArray alpha_copy;
      alpha_copy.copy_from(*p_alpha, node.cfg().cm_cpu);

      hmap::for_each_tile(
          {&alpha_copy, p_noise},
          [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
          {
            hmap::Array &alpha    = *p_arrays[0];
            hmap::Array *pa_noise = p_arrays[1];

            if (pa_noise)
              alpha += *pa_noise;

            if (node.val<bool>(A_CLAMP))
              hmap::clamp(alpha, 0.f, 1.f);

            if (node.val<bool>(A_REVERSE))
              alpha = 1.f - alpha;
          },
          node.cfg().cm_cpu);

      p_tex->channel(3).copy_from(alpha_copy, node.cfg().cm_cpu);
    }
    else
    {
      float alpha = node.val<float>(A_ALPHA);

      if (node.val<bool>(A_REVERSE))
        alpha = 1.f - alpha;

      p_tex->fill(3, alpha, node.cfg().cm_cpu);
    }
  }
}

} // namespace hesiod
