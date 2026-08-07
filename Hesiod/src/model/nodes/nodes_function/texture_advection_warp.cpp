/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <fstream>

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
constexpr const char *P_ELEVATION = "elevation";
constexpr const char *P_IN        = "input";
constexpr const char *P_MASK      = "mask";
constexpr const char *P_TEXTURE   = "texture";

constexpr const char *A_ADVECTION_LENGTH  = "advection_length";
constexpr const char *A_VALUE_PERSISTENCE = "value_persistence";

void setup_texture_advection_warp_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ELEVATION);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT, P_TEXTURE, CONFIG_TEX(node));

  // attribute(s)
  add_float(node, A_ADVECTION_LENGTH, "advection_length", 0.05f, 0.f, 0.2f);
  add_float(node, A_VALUE_PERSISTENCE, "value_persistence", 0.95f, 0.8f, 1.f);

  setup_pre_process_mask_attributes(node);
}

void compute_texture_advection_warp_node(BaseNode &node)
{
  Logger::log()->error("TextureAdvectionWarp node is deprecated, use "
                       "TextureAdvectionParticle node");

  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray   *p_z   = node.get_value_ref<hmap::VirtualArray>(P_ELEVATION);
  hmap::VirtualTexture *p_tex = node.get_value_ref<hmap::VirtualTexture>(P_IN);

  if (p_z && p_tex)
  {
    hmap::VirtualArray   *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);
    hmap::VirtualTexture *p_out  = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE);

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
                node.val<float>(A_ADVECTION_LENGTH),
                node.val<float>(A_VALUE_PERSISTENCE),
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
