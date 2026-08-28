/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/colorize.hpp"
#include "highmap/range.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "meta/metadata/keys.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/constants/color_gradient.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_INPUT1  = "input1";
constexpr const char *P_INPUT2  = "input2";
constexpr const char *P_NOISE   = "noise";
constexpr const char *P_TEXTURE = "texture";

constexpr const char *A_GRADIENT1         = "gradient1";
constexpr const char *A_GRADIENT2         = "gradient2";
constexpr const char *A_MIX_METHOD        = "mix_method";
constexpr const char *A_REVERSE_COLORMAP1 = "reverse_colormap1";
constexpr const char *A_REVERSE_COLORMAP2 = "reverse_colormap2";
constexpr const char *A_SAT_PERC1         = "sat_percentile1";
constexpr const char *A_SAT_PERC2         = "sat_percentile2";
constexpr const char *A_SHARPNESS         = "sharpness";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_colorize_bivariate_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_INPUT1);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_INPUT2);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_NOISE);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT, P_TEXTURE, CONFIG_TEX(node));

  // --- Attributes

  auto presets = meta::GradientPresets{
      ColorGradientManager::get_instance().get_as_attr_presets()};

  // clang-format off
  node.set_current_category("Colormap 1");
  add_color_gradient(node, A_GRADIENT1, "Gradient");
  node.set_metadata(A_GRADIENT1, meta::keys::ui::presets, presets);
  add_bool(node, A_REVERSE_COLORMAP1, "Reverse Colormap", false);
  add_float(node, A_SAT_PERC1, "Saturation Percentile", 0.f, 0.f, 50.f, "{:.1f}%");

  node.set_current_category("Colormap 2");
  add_color_gradient(node, A_GRADIENT2, "Gradient");
  node.set_metadata(A_GRADIENT2, meta::keys::ui::presets, presets);
  add_bool(node, A_REVERSE_COLORMAP2, "Reverse Colormap", false);
  add_float(node, A_SAT_PERC2, "Saturation Percentile", 0.f, 0.f, 50.f, "{:.1f}%");

  node.set_current_category("Blending");
  add_enum(node, A_MIX_METHOD, "Color Mix Method", enum_mappings.mix_method_map, "Square Averaged");
  add_float(node, A_SHARPNESS, "Sharpness", 1.f, 0.01f, 10.f);
  // clang-format on
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_colorize_bivariate_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in1   = node.get_value_ref<hmap::VirtualArray>(P_INPUT1);
  auto *p_in2   = node.get_value_ref<hmap::VirtualArray>(P_INPUT2);
  auto *p_noise = node.get_value_ref<hmap::VirtualArray>(P_NOISE);
  auto *p_tex   = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE);

  if (!p_in1 || !p_in2)
    return;

  // --- Params

  const auto &gradient1  = node.val<meta::ColorGradient>(A_GRADIENT1).value();
  const auto &gradient2  = node.val<meta::ColorGradient>(A_GRADIENT2).value();
  const auto  mix_method = static_cast<hmap::MixMethod>(node.val<int>(A_MIX_METHOD));
  const auto  reverse_colormap1 = node.val<bool>(A_REVERSE_COLORMAP1);
  const auto  reverse_colormap2 = node.val<bool>(A_REVERSE_COLORMAP2);
  const auto  sat_perc1         = 0.01f * node.val<float>(A_SAT_PERC1);
  const auto  sat_perc2         = 0.01f * node.val<float>(A_SAT_PERC2);
  const auto  sharpness         = node.val<float>(A_SHARPNESS);

  std::vector<float>     positions1       = {};
  std::vector<glm::vec3> colormap_colors1 = {};

  for (const auto &data : gradient1)
  {
    positions1.push_back(data.position);
    colormap_colors1.push_back({data.color[0], data.color[1], data.color[2]});
  }

  std::vector<float>     positions2       = {};
  std::vector<glm::vec3> colormap_colors2 = {};

  for (const auto &data : gradient2)
  {
    positions2.push_back(data.position);
    colormap_colors2.push_back({data.color[0], data.color[1], data.color[2]});
  }

  // --- Ranges with saturation percentiles

  glm::vec2 range1 = (sat_perc1 > 0.f) ? p_in1->range_percentile(sat_perc1,
                                                                 1.f - sat_perc1,
                                                                 node.cfg().cm_cpu)
                                       : p_in1->range(node.cfg().cm_cpu);

  glm::vec2 range2 = (sat_perc2 > 0.f) ? p_in2->range_percentile(sat_perc2,
                                                                 1.f - sat_perc2,
                                                                 node.cfg().cm_cpu)
                                       : p_in2->range(node.cfg().cm_cpu);

  if (p_noise)
  {
    float nmin = p_noise->min(node.cfg().cm_cpu);
    float nmax = p_noise->max(node.cfg().cm_cpu);
    range1.x += nmin;
    range1.y += nmax;
    range2.x += nmin;
    range2.y += nmax;
  }

  hmap::VirtualArray  in1_processed;
  hmap::VirtualArray  in2_processed;
  hmap::VirtualArray *p_in1_final   = p_in1;
  hmap::VirtualArray *p_in2_final   = p_in2;
  hmap::VirtualArray *p_noise_final = p_noise;

  glm::vec2 range1_final = range1;
  glm::vec2 range2_final = range2;

  if (sharpness != 1.f)
  {
    in1_processed.copy_from(*p_in1, node.cfg().cm_cpu, false);
    in2_processed.copy_from(*p_in2, node.cfg().cm_cpu, false);

    float denom1 = (range1.y != range1.x) ? (range1.y - range1.x) : 1.f;
    float denom2 = (range2.y != range2.x) ? (range2.y - range2.x) : 1.f;

    std::vector<hmap::VirtualArray *> ptrs = {&in1_processed,
                                              &in2_processed,
                                              p_in1,
                                              p_in2,
                                              p_noise};

    hmap::for_each_tile(
        ptrs,
        [range1, range2, denom1, denom2, sharpness](std::vector<hmap::Array *> p_arrays,
                                                    const hmap::TileRegion    &region)
        {
          auto [pa_out1, pa_out2, pa_in1, pa_in2, pa_noise] = unpack<5>(p_arrays);

          for (int j = 0; j < region.shape.y; ++j)
          {
            for (int i = 0; i < region.shape.x; ++i)
            {
              float noise_val = pa_noise ? (*pa_noise)(i, j) : 0.f;

              float v1 = (*pa_in1)(i, j) + noise_val;
              float v2 = (*pa_in2)(i, j) + noise_val;

              v1 = (v1 - range1.x) / denom1;
              v2 = (v2 - range2.x) / denom2;

              v1 = std::clamp(v1, 0.f, 1.f);
              v2 = std::clamp(v2, 0.f, 1.f);

              v1 = hmap::gain(v1, sharpness);
              v2 = hmap::gain(v2, sharpness);

              (*pa_out1)(i, j) = v1;
              (*pa_out2)(i, j) = v2;
            }
          }
        },
        node.cfg().cm_cpu);

    p_in1_final   = &in1_processed;
    p_in2_final   = &in2_processed;
    range1_final  = {0.f, 1.f};
    range2_final  = {0.f, 1.f};
    p_noise_final = nullptr;
  }

  // --- Compute

  hmap::colorize_bivariate(*p_tex,
                           *p_in1_final,
                           *p_in2_final,
                           node.cfg().cm_cpu,
                           range1_final,
                           range2_final,
                           positions1,
                           positions2,
                           colormap_colors1,
                           colormap_colors2,
                           mix_method,
                           reverse_colormap1,
                           reverse_colormap2,
                           p_noise_final,
                           p_noise_final);
}

} // namespace hesiod
