/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/primitives.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_NOISE       = "noise";
constexpr const char *P_ENVELOPE    = "envelope";
constexpr const char *P_OUT         = "output";
constexpr const char *P_CRATER_MASK = "crater_mask";

constexpr const char *A_RADIUS              = "radius";
constexpr const char *A_ANGLE               = "angle";
constexpr const char *A_CENTER              = "center";
constexpr const char *A_ELEVATION_OFFSET    = "elevation_offset";
constexpr const char *A_INNER_DEPTH         = "inner_depth";
constexpr const char *A_INNER_EXP           = "inner_exp";
constexpr const char *A_LIP_HEIGHT          = "lip_height";
constexpr const char *A_LIP_EXTENT          = "lip_extent";
constexpr const char *A_LIP_EXP             = "lip_exp";
constexpr const char *A_ASYM_RATIO          = "asym_ratio";
constexpr const char *A_CENTRAL_PEAK_HEIGHT = "central_peak_height";
constexpr const char *A_CENTRAL_PEAK_EXTENT = "central_peak_extent";
constexpr const char *A_N_TERRACES          = "n_terraces";
constexpr const char *A_TERRACE_EXTENT      = "terrace_extent";
constexpr const char *A_TERRACE_EXP         = "terrace_exp";
constexpr const char *A_TERRACE_PERSISTENCE = "terrace_persistence";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_crater_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_NOISE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ENVELOPE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_CRATER_MASK, CONFIG(node));

  // --- Attributes

  // clang-format off
  node.add_attr<FloatAttribute>(A_RADIUS, "Crater Radius", 0.25f, 0.f, 2.f);
  node.add_attr<FloatAttribute>(A_ANGLE, "Rotation Angle", 0.f, -180.f, 180.f, "{:.1f}°");
  node.add_attr<Vec2FloatAttribute>(A_CENTER, "Center");
  node.add_attr<FloatAttribute>(A_ELEVATION_OFFSET, "Elevation Offset", 0.f, -1.f, 1.f);
  node.add_attr<FloatAttribute>(A_INNER_DEPTH, "Inner Basin Depth", 0.2f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_INNER_EXP, "Inner Basin Falloff", 2.5f, 0.01f, 4.f);
  node.add_attr<FloatAttribute>(A_LIP_HEIGHT, "Rim Height", 0.05f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_LIP_EXTENT, "Rim Width", 0.15f, 0.f, 2.f);
  node.add_attr<FloatAttribute>(A_LIP_EXP, "Rim Falloff", 2.f, 0.01f, 4.f);
  node.add_attr<FloatAttribute>(A_ASYM_RATIO, "Asymmetry Ratio", 1.f, 0.f, 4.f);
  node.add_attr<FloatAttribute>(A_CENTRAL_PEAK_HEIGHT, "Central Peak Height", 0.01f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_CENTRAL_PEAK_EXTENT, "Central Peak Extent", 0.4f, 0.f, 1.f);
  node.add_attr<IntAttribute>(A_N_TERRACES, "Terrace Count", 0, 0, 16);
  node.add_attr<FloatAttribute>(A_TERRACE_EXTENT, "Terrace Extent", 0.1f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_TERRACE_EXP, "Terrace Sharpness", 2.f, 0.01f, 1.f);
  node.add_attr<FloatAttribute>(A_TERRACE_PERSISTENCE, "Terrace Persistence", 0.5f, 0.f, 1.f);
  // clang-format on

  // --- Attribute(s) order

  // attribute(s) order
  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Global Shape",
                             A_RADIUS,
                             A_ANGLE,
                             A_CENTER,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Base Profile",
                             A_INNER_DEPTH,
                             A_INNER_EXP,
                             A_ELEVATION_OFFSET,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Rim Profile",
                             A_LIP_HEIGHT,
                             A_LIP_EXTENT,
                             A_LIP_EXP,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Shape Distortion",
                             A_ASYM_RATIO,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Rim Profile",
                             A_CENTRAL_PEAK_HEIGHT,
                             A_CENTRAL_PEAK_EXTENT,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Shape Distortion",
                             A_N_TERRACES,
                             A_TERRACE_EXTENT,
                             A_TERRACE_EXP,
                             A_TERRACE_PERSISTENCE,
                             "_GROUPBOX_END_"});

  setup_default_noise(node, {.noise_amp = 0.2f, .kw = 4.f, .smoothness = 0.3f});

  setup_post_process_heightmap_attributes(
      node,
      {.add_mix = false, .remap_active_state = false});
}

void compute_crater_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_noise       = node.get_value_ref<hmap::VirtualArray>(P_NOISE);
  auto *p_envelope    = node.get_value_ref<hmap::VirtualArray>(P_ENVELOPE);
  auto *p_out         = node.get_value_ref<hmap::VirtualArray>(P_OUT);
  auto *p_crater_mask = node.get_value_ref<hmap::VirtualArray>(P_CRATER_MASK);

  // --- Params

  // clang-format off
  const auto radius              = node.get_attr<FloatAttribute>(A_RADIUS);
  const auto angle               = node.get_attr<FloatAttribute>(A_ANGLE);
  const auto center              = node.get_attr<Vec2FloatAttribute>(A_CENTER);
  const auto elevation_offset    = node.get_attr<FloatAttribute>(A_ELEVATION_OFFSET);
  const auto inner_depth         = node.get_attr<FloatAttribute>(A_INNER_DEPTH);
  const auto inner_exp           = node.get_attr<FloatAttribute>(A_INNER_EXP);
  const auto lip_height          = node.get_attr<FloatAttribute>(A_LIP_HEIGHT);
  const auto lip_extent          = node.get_attr<FloatAttribute>(A_LIP_EXTENT);
  const auto lip_exp             = node.get_attr<FloatAttribute>(A_LIP_EXP);
  const auto asym_ratio          = node.get_attr<FloatAttribute>(A_ASYM_RATIO);
  const auto central_peak_height = node.get_attr<FloatAttribute>(A_CENTRAL_PEAK_HEIGHT);
  const auto central_peak_extent = node.get_attr<FloatAttribute>(A_CENTRAL_PEAK_EXTENT);
  const auto n_terraces          = node.get_attr<IntAttribute>(A_N_TERRACES);
  const auto terrace_extent      = node.get_attr<FloatAttribute>(A_TERRACE_EXTENT);
  const auto terrace_exp         = node.get_attr<FloatAttribute>(A_TERRACE_EXP);
  const auto terrace_persistence = node.get_attr<FloatAttribute>(A_TERRACE_PERSISTENCE);
  // clang-format on

  // --- Resolve default noise

  hmap::VirtualArray noise_default(CONFIG(node));
  generate_noise(node, p_noise, noise_default);

  // --- Compute

  hmap::for_each_tile(
      {p_noise, p_envelope},
      {p_out, p_crater_mask},
      [&](std::vector<const hmap::Array *> in,
          std::vector<hmap::Array *>       out,
          const hmap::TileRegion          &region)
      {
        auto [pa_noise, pa_envelope]  = unpack<2>(in);
        auto [pa_out, pa_crater_mask] = unpack<2>(out);

        *pa_out = hmap::crater(region.shape,
                               radius,
                               center,
                               angle,
                               inner_depth,
                               inner_exp,
                               lip_height,
                               lip_extent,
                               lip_exp,
                               asym_ratio,
                               central_peak_height,
                               central_peak_extent,
                               n_terraces,
                               terrace_extent,
                               terrace_exp,
                               terrace_persistence,
                               pa_noise,
                               region.bbox,
                               pa_crater_mask,
                               /* pa_inner_crater_mask */ nullptr);

        if (pa_envelope)
          *pa_out *= *pa_envelope;

        if (elevation_offset)
          *pa_out += elevation_offset;
      },
      node.cfg().cm_cpu);

  // --- Post-process

  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
