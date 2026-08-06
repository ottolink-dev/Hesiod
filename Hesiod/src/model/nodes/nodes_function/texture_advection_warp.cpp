/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <fstream>

#include "highmap/transform.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_texture_advection_warp_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "elevation");
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "mask");
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT, "texture", CONFIG_TEX(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("advection_length", "advection_length", 0.05f, 0.f, 0.2f);
  node.add_attr<FloatAttribute>("value_persistence",
                                "value_persistence",
                                0.95f,
                                0.8f,
                                1.f);

  // attribute(s) order
  node.set_attr_ordered_key({"advection_length", "value_persistence"});

  setup_pre_process_mask_attributes(node);
}

void compute_texture_advection_warp_node(BaseNode &node)
{
  Logger::log()->error(
      "TextureAdvectionWarp node is deprecated, use TextureAdvectionParticle node");

  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray   *p_z = node.get_value_ref<hmap::VirtualArray>("elevation");
  hmap::VirtualTexture *p_tex = node.get_value_ref<hmap::VirtualTexture>("input");

  if (p_z && p_tex)
  {
    hmap::VirtualArray   *p_mask = node.get_value_ref<hmap::VirtualArray>("mask");
    hmap::VirtualTexture *p_out = node.get_value_ref<hmap::VirtualTexture>("texture");

    // prepare mask
    std::shared_ptr<hmap::VirtualArray> sp_mask = pre_process_mask(node, p_mask, *p_z);

    // apply advection separetely to each RGBA channels
    auto lambda = [&node](hmap::VirtualArray *p_field_out,
                          hmap::VirtualArray *p_z,
                          hmap::VirtualArray *p_field,
                          hmap::VirtualArray *p_mask)
    {
      hmap::for_each_tile(
          {p_field_out, p_z, p_field, p_mask},
          [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
          {
            auto [pa_field_out, pa_z, pa_field, pa_mask] = unpack<4>(p_arrays);

            *pa_field_out = hmap::gpu::advection_warp(
                *pa_z,
                *pa_field,
                node.get_attr<FloatAttribute>("advection_length"),
                node.get_attr<FloatAttribute>("value_persistence"),
                pa_mask);
          },
          node.cfg().cm_gpu);
    };

    for (int nch = 0; nch < 4; nch++)
    {
      lambda(&(p_out->channel(nch)), p_z, &(p_tex->channel(nch)), p_mask);
      p_out->channel(nch).smooth_overlap_buffers();
    }
  }
}

} // namespace hesiod
