/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/colorize.hpp"
#include "highmap/kernels.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_TEXTURE  = "texture";
constexpr const char *P_TEXTURE1 = "texture1";
constexpr const char *P_TEXTURE2 = "texture2";
constexpr const char *P_TEXTURE3 = "texture3";
constexpr const char *P_TEXTURE4 = "texture4";

constexpr const char *A_RESET_OUTPUT_ALPHA = "reset_output_alpha";
constexpr const char *A_MIX_METHOD         = "mix_method";

void setup_mix_texture_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_TEXTURE1);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_TEXTURE2);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_TEXTURE3);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_TEXTURE4);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT, P_TEXTURE, CONFIG_TEX(node));

  // attribute(s)
  add_enum(node, A_MIX_METHOD, "mix_method", EnumMappings{}.mix_method_map, "sqrt_avg");
  add_bool(node, A_RESET_OUTPUT_ALPHA, "reset_output_alpha", true);
}

void compute_mix_texture_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualTexture *p_in1 = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE1);
  hmap::VirtualTexture *p_in2 = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE2);
  hmap::VirtualTexture *p_in3 = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE3);
  hmap::VirtualTexture *p_in4 = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE4);
  hmap::VirtualTexture *p_out = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE);

  std::vector<hmap::VirtualTexture *> ptr_list = {};

  for (auto &ptr : {p_in1, p_in2, p_in3, p_in4})
    if (ptr)
      ptr_list.push_back(ptr);

  if ((int)ptr_list.size())
  {
    const auto   mix_method = node.val_enum<hmap::MixMethod>(A_MIX_METHOD);
    const size_t n_tex      = ptr_list.size();

    std::vector<const hmap::VirtualArray *> inputs;
    for (auto *p_tex : ptr_list)
      for (auto *p_ch : p_tex->channels_ptr())
        inputs.push_back(p_ch);

    hmap::for_each_tile(
        inputs,
        p_out->channels_ptr(),
        [&](std::vector<const hmap::Array *> p_arrays_in,
            std::vector<hmap::Array *>       p_arrays_out,
            const hmap::UvTile &)
        {
          const size_t                       n_ch = p_arrays_out.size();
          std::vector<hmap::Texture>         textures(n_tex);
          std::vector<const hmap::Texture *> tex_ptrs;
          tex_ptrs.reserve(n_tex);

          for (size_t t = 0; t < n_tex; ++t)
          {
            textures[t].shape = p_arrays_in[t * n_ch]->shape;
            textures[t].channels.reserve(n_ch);
            for (size_t ch = 0; ch < n_ch; ++ch)
              textures[t].channels.push_back(*p_arrays_in[t * n_ch + ch]);
            tex_ptrs.push_back(&textures[t]);
          }

          hmap::Texture tex_out = hmap::mix(tex_ptrs, mix_method);

          for (size_t ch = 0; ch < n_ch && ch < tex_out.channels.size(); ++ch)
            *p_arrays_out[ch] = std::move(tex_out.channels[ch]);
        },
        node.cfg().cm_cpu);

    p_out->sync_overlap_buffers();

    if (node.val<bool>(A_RESET_OUTPUT_ALPHA))
    {
      if (p_out->channels_ptr().size() >= 4)
        p_out->fill(3, 1.f, node.cfg().cm_cpu);
      else
        Logger::log()->error("compute_mix_texture_node: cannot reset alpha, expected 4 "
                             "channels but got {}",
                             p_out->channels_ptr().size());
    }
  }
}

} // namespace hesiod
