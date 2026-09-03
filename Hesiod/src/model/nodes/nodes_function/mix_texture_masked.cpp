/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/blending.hpp"
#include "highmap/colorize.hpp"
#include "highmap/kernels.hpp"
#include "highmap/range.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_MASK     = "mask";
constexpr const char *P_TEXTURE  = "texture";
constexpr const char *P_TEXTURE1 = "texture1";
constexpr const char *P_TEXTURE2 = "texture2";

constexpr const char *A_GAIN           = "gain";
constexpr const char *A_ITERATIONS     = "iterations";
constexpr const char *A_MASK_THRESHOLD = "mask_threshold";
constexpr const char *A_MIX_METHOD     = "mix_method";

constexpr const char *G_POISSON      = "Poisson";
constexpr const char *G_TRANSPARENCY = "Transparency";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_mix_texture_masked_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_TEXTURE1);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_TEXTURE2);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT, P_TEXTURE, CONFIG_TEX(node));

  // Group: Transparency
  {
    node.set_current_group(G_TRANSPARENCY);
    add_enum(node,
             A_MIX_METHOD,
             "Color Mix Method",
             enum_mappings.mix_method_map,
             "Square Averaged");
    add_float(node, A_GAIN, "Gain", 1.f, 0.01f, 10.f);
  }

  // Group: Poisson
  {
    node.set_current_group(G_POISSON);
    add_int(node, A_ITERATIONS, "Iterations", 100, 1, 2000);
    add_float(node, A_MASK_THRESHOLD, "Mask Threshold", 0.f, 0.f, 1.f);
    add_float(node, A_GAIN, "Gain", 1.f, 0.01f, 10.f);
  }

  // Reset active group to default
  node.set_current_group(G_TRANSPARENCY);
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_mix_texture_masked_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualTexture *p_in1  = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE1);
  hmap::VirtualTexture *p_in2  = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE2);
  hmap::VirtualArray   *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  hmap::VirtualTexture *p_out  = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE);

  if (!p_in1 || !p_in2 || !p_out)
    return;

  const std::string current_group = node.get_meta_group()
                                        .current_container_name()
                                        .value_or(G_TRANSPARENCY);

  std::vector<hmap::VirtualArray *> vas = {};
  for (auto *p : p_out->channels_ptr())
    vas.push_back(p);
  for (auto *p : p_in1->channels_ptr())
    vas.push_back(p);
  for (auto *p : p_in2->channels_ptr())
    vas.push_back(p);
  vas.push_back(p_mask);

  if (current_group == G_TRANSPARENCY)
  {
    const auto mix_method = static_cast<hmap::MixMethod>(node.val<int>(A_MIX_METHOD));
    const auto gain       = node.val<float>(A_GAIN);

    auto lambda =
        [mix_method, gain](std::vector<hmap::Array *> &p_arrays, const hmap::TileRegion &)
    {
      // Out: 0..3
      // In1: 4..7
      // In2: 8..11
      // Mask: 12
      hmap::Array *pa_mask = p_arrays[12];

      hmap::Texture t1(*p_arrays[4], *p_arrays[5], *p_arrays[6], *p_arrays[7]);
      hmap::Texture t2(*p_arrays[8], *p_arrays[9], *p_arrays[10], *p_arrays[11]);

      hmap::Texture blended = pa_mask ? hmap::mix(t1, t2, *pa_mask, mix_method, gain)
                                      : hmap::mix(t1, t2, mix_method);

      for (int c = 0; c < 4; ++c)
      {
        if (c < blended.num_channels())
          *p_arrays[c] = blended[c];
      }
    };

    hmap::for_each_tile(vas, lambda, node.cfg().cm_cpu);
  }
  else // Poisson
  {
    const auto iterations = node.val<int>(A_ITERATIONS);
    const auto threshold  = node.val<float>(A_MASK_THRESHOLD);
    const auto gain       = node.val<float>(A_GAIN);

    auto lambda = [iterations, threshold, gain](std::vector<hmap::Array *> &p_arrays,
                                                const hmap::TileRegion &)
    {
      // Out: 0..3
      // In1: 4..7
      // In2: 8..11
      // Mask: 12
      hmap::Array *pa_mask = p_arrays[12];

      hmap::Texture t1(*p_arrays[4], *p_arrays[5], *p_arrays[6], *p_arrays[7]);
      hmap::Texture t2(*p_arrays[8], *p_arrays[9], *p_arrays[10], *p_arrays[11]);

      hmap::Texture blended;

      if (pa_mask)
      {
        hmap::Array mask_proc = *pa_mask;

        if (threshold != 0.f)
        {
          mask_proc -= threshold;
          hmap::clamp_min(mask_proc, 0.f);
        }
        if (gain != 1.f)
        {
          hmap::gain(mask_proc, gain);
        }

        blended = hmap::gpu::blend_poisson_bf(t1, t2, iterations, &mask_proc);
      }
      else
      {
        blended = hmap::gpu::blend_poisson_bf(t1, t2, iterations);
      }

      for (int c = 0; c < 4; ++c)
      {
        if (c < blended.num_channels())
          *p_arrays[c] = blended[c];
      }
    };

    hmap::for_each_tile(vas, lambda, node.cfg().cm_gpu);

    for (int c = 0; c < p_out->channels(); ++c)
      p_out->channel(c).smooth_overlap_buffers();
  }
}

} // namespace hesiod
