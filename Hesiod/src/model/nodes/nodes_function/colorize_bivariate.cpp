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

  add_color_gradient(node, A_GRADIENT1, "gradient1");
  node.set_metadata(
      A_GRADIENT1,
      meta::keys::ui::presets,
      meta::GradientPresets{ColorGradientManager::get_instance().get_as_attr_presets()});

  add_color_gradient(node, A_GRADIENT2, "gradient2");
  node.set_metadata(
      A_GRADIENT2,
      meta::keys::ui::presets,
      meta::GradientPresets{ColorGradientManager::get_instance().get_as_attr_presets()});

  add_enum(node,
           A_MIX_METHOD,
           "mix_method",
           enum_mappings.mix_method_map,
           "Square Averaged");

  add_bool(node, A_REVERSE_COLORMAP1, "reverse_colormap1", false);
  add_bool(node, A_REVERSE_COLORMAP2, "reverse_colormap2", false);

  add_float(node, A_SAT_PERC1, "Saturation Percentile 1", 0.f, 0.f, 50.f, "{:.1f}%");
  add_float(node, A_SAT_PERC2, "Saturation Percentile 2", 0.f, 0.f, 50.f, "{:.1f}%");
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

  const auto &gradient1 = node.val<meta::ColorGradient>(A_GRADIENT1).value();
  const auto &gradient2 = node.val<meta::ColorGradient>(A_GRADIENT2).value();

  const auto mix_method = static_cast<hmap::MixMethod>(node.val<int>(A_MIX_METHOD));
  const auto reverse_colormap1 = node.val<bool>(A_REVERSE_COLORMAP1);
  const auto reverse_colormap2 = node.val<bool>(A_REVERSE_COLORMAP2);

  const auto sat_perc1 = 0.01f * node.val<float>(A_SAT_PERC1);
  const auto sat_perc2 = 0.01f * node.val<float>(A_SAT_PERC2);

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

  // --- Compute

  hmap::colorize_bivariate(*p_tex,
                           *p_in1,
                           *p_in2,
                           node.cfg().cm_cpu,
                           range1,
                           range2,
                           positions1,
                           positions2,
                           colormap_colors1,
                           colormap_colors2,
                           mix_method,
                           reverse_colormap1,
                           reverse_colormap2,
                           p_noise,
                           p_noise);
}

} // namespace hesiod
