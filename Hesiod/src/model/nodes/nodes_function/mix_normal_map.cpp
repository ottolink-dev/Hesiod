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
constexpr const char *P_NORMAL_MAP        = "normal map";
constexpr const char *P_NORMAL_MAP_BASE   = "normal map base";
constexpr const char *P_NORMAL_MAP_DETAIL = "normal map detail";

constexpr const char *A_BLENDING_METHOD = "blending_method";
constexpr const char *A_DETAIL_SCALING  = "detail_scaling";

void setup_mix_normal_map_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_NORMAL_MAP_BASE);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_NORMAL_MAP_DETAIL);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT,
                                      P_NORMAL_MAP,
                                      CONFIG_TEX(node));

  // attribute(s)
  add_float(node, A_DETAIL_SCALING, "detail_scaling", 1.f, 0.f, 4.f);
  add_enum(node,
           A_BLENDING_METHOD,
           "blending_method",
           hmap::normal_map_blending_method_as_string);
}

void compute_mix_normal_map_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualTexture *p_in1 = node.get_value_ref<hmap::VirtualTexture>(
      P_NORMAL_MAP_BASE);
  hmap::VirtualTexture *p_in2 = node.get_value_ref<hmap::VirtualTexture>(
      P_NORMAL_MAP_DETAIL);

  if (p_in1 && p_in2)
  {
    hmap::VirtualTexture *p_out = node.get_value_ref<hmap::VirtualTexture>(P_NORMAL_MAP);

    const float detail_scaling  = node.val<float>(A_DETAIL_SCALING);
    const auto  blending_method = node.val_enum<hmap::NormalMapBlendingMethod>(
        A_BLENDING_METHOD);

    std::vector<const hmap::VirtualArray *> inputs;
    for (auto *p : p_in1->channels_ptr())
      inputs.push_back(p);
    for (auto *p : p_in2->channels_ptr())
      inputs.push_back(p);

    hmap::for_each_tile(
        inputs,
        p_out->channels_ptr(),
        [&](std::vector<const hmap::Array *> p_arrays_in,
            std::vector<hmap::Array *>       p_arrays_out,
            const hmap::UvTile &)
        {
          const size_t  n_ch = p_arrays_out.size();
          hmap::Texture tex_base;
          hmap::Texture tex_detail;
          tex_base.shape   = p_arrays_in[0]->shape;
          tex_detail.shape = p_arrays_in[n_ch]->shape;
          tex_base.channels.reserve(n_ch);
          tex_detail.channels.reserve(n_ch);
          for (size_t ch = 0; ch < n_ch; ++ch)
          {
            tex_base.channels.push_back(*p_arrays_in[ch]);
            tex_detail.channels.push_back(*p_arrays_in[n_ch + ch]);
          }

          hmap::Texture tex_out = hmap::mix_normal_map(tex_base,
                                                       tex_detail,
                                                       detail_scaling,
                                                       blending_method);

          for (size_t ch = 0; ch < n_ch && ch < tex_out.channels.size(); ++ch)
            *p_arrays_out[ch] = std::move(tex_out.channels[ch]);
        },
        node.cfg().cm_cpu);

    p_out->sync_overlap_buffers();
  }
}

} // namespace hesiod
