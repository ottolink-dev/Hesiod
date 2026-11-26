/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/colorize.hpp"
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
constexpr const char *P_TEXTURE   = "texture";

void setup_texture_to_heightmap_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_TEXTURE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_ELEVATION, CONFIG(node));

  // attribute(s)

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

void compute_texture_to_heightmap_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualTexture *p_tex = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE);

  if (p_tex)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_ELEVATION);

    std::vector<const hmap::VirtualArray *> inputs;
    for (auto *p_ch : p_tex->channels_ptr())
      inputs.push_back(p_ch);

    hmap::for_each_tile(
        inputs,
        {p_out},
        [&](std::vector<const hmap::Array *> p_arrays_in,
            std::vector<hmap::Array *>       p_arrays_out,
            const hmap::UvTile &)
        {
          auto [pa_out] = unpack<1>(p_arrays_out);

          hmap::Texture tex;
          tex.channels.reserve(p_arrays_in.size());
          for (const auto *pa_ch : p_arrays_in)
            tex.channels.push_back(*pa_ch);

          *pa_out = hmap::luminance(tex);
        },
        node.cfg().cm_cpu);

    p_out->sync_overlap_buffers();

    // post-process
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
