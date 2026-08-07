/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/range.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_set_alpha_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, "texture in");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "alpha");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "noise");
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT,
                                      "texture out",
                                      CONFIG_TEX(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("alpha", "alpha", 1.f, 0.f, 1.f);
  node.add_attr<BoolAttribute>("reverse", "reverse", false);
  node.add_attr<BoolAttribute>("clamp", "clamp", true);

  // attribute(s) order
  node.set_attr_ordered_key({"alpha", "reverse", "clamp"});
}

void compute_set_alpha_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualTexture *p_in = node.get_value_ref<hmap::VirtualTexture>("texture in");

  if (p_in)
  {
    hmap::VirtualArray   *p_alpha = node.get_value_ref<hmap::VirtualArray>("alpha");
    hmap::VirtualArray   *p_noise = node.get_value_ref<hmap::VirtualArray>("noise");
    hmap::VirtualTexture *p_tex = node.get_value_ref<hmap::VirtualTexture>("texture out");

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

            if (node.get_attr<BoolAttribute>("clamp"))
              hmap::clamp(alpha, 0.f, 1.f);

            if (node.get_attr<BoolAttribute>("reverse"))
              alpha = 1.f - alpha;
          },
          node.cfg().cm_cpu);

      p_tex->channel(3).copy_from(alpha_copy, node.cfg().cm_cpu);
    }
    else
    {
      float alpha = node.get_attr<FloatAttribute>("alpha");

      if (node.get_attr<BoolAttribute>("reverse"))
        alpha = 1.f - alpha;

      p_tex->fill(3, alpha, node.cfg().cm_cpu);
    }
  }
}

} // namespace hesiod
