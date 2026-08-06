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

constexpr const char *P_DR = "dr";
constexpr const char *P_ENV = "envelope";
constexpr const char *P_OUT = "output";

constexpr const char *A_KW = "kw";
constexpr const char *A_ANGLE = "angle";
constexpr const char *A_PHASE_SHIFT = "phase_shift";
constexpr const char *A_CENTER = "center";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_wave_sine_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DR);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ENV);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  node.add_attr<FloatAttribute>(A_KW, "kw", 2.f, 0.01f, FLT_MAX);
  node.add_attr<FloatAttribute>(A_ANGLE, "angle", 0.f, -180.f, 180.f, "{:.1f}°");
  node.add_attr<FloatAttribute>(A_PHASE_SHIFT,
                                "phase_shift",
                                0.f,
                                -180.f,
                                180.f,
                                "{:.1f}°");
  node.add_attr<Vec2FloatAttribute>(A_CENTER, "center");

  // --- Attribute(s) order

  node.set_attr_ordered_key({
      "_GROUPBOX_BEGIN_Wave Parameters",
      A_KW,
      A_ANGLE,
      A_PHASE_SHIFT,
      A_CENTER,
      "_GROUPBOX_END_",
  });

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_wave_sine_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_dr = node.get_value_ref<hmap::VirtualArray>(P_DR);
  auto *p_env = node.get_value_ref<hmap::VirtualArray>(P_ENV);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_out)
    return;

  // --- Params

  // clang-format off
  const auto kw          = node.get_attr<FloatAttribute>(A_KW);
  const auto angle       = node.get_attr<FloatAttribute>(A_ANGLE);
  const auto phase_shift = node.get_attr<FloatAttribute>(A_PHASE_SHIFT);
  const auto center      = node.get_attr<Vec2FloatAttribute>(A_CENTER);
  // clang-format on

  // phase_shift slider is in degrees (range -180..180, matching `angle`);
  // hmap::wave_sine expects radians in cos(2*pi*r + phase). Convert here.
  const float phase_rad = phase_shift * float(M_PI / 180.0);

  // --- Compute

  hmap::for_each_tile(
      {p_dr},
      {p_out},
      [&](std::vector<const hmap::Array *> in,
          std::vector<hmap::Array *>       out,
          const hmap::TileRegion          &region)
      {
        auto [pa_dr] = unpack<1>(in);
        auto [pa_out] = unpack<1>(out);

        *pa_out = hmap::wave_sine(region.shape,
                                  kw,
                                  angle,
                                  phase_rad,
                                  pa_dr,
                                  nullptr,
                                  nullptr,
                                  center,
                                  region.bbox);
      },
      node.cfg().cm_cpu);

  // --- Post-process

  post_apply_enveloppe(node, *p_out, p_env);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
