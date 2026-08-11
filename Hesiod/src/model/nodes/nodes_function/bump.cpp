/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/primitives.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_DX   = "dx";
constexpr const char *P_DY   = "dy";
constexpr const char *P_CTRL = "control";
constexpr const char *P_ENV  = "envelope";
constexpr const char *P_OUT  = "output";

constexpr const char *A_GAIN   = "gain";
constexpr const char *A_CENTER = "center";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_bump_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_CTRL);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ENV);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  node.set_current_category("Main Parameters");
  add_float(node, A_GAIN, "gain", 1.f, 0.01f, 10.f);
  add_xy(node, A_CENTER, "center");

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_bump_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_dx   = node.get_value_ref<hmap::VirtualArray>(P_DX);
  auto *p_dy   = node.get_value_ref<hmap::VirtualArray>(P_DY);
  auto *p_ctrl = node.get_value_ref<hmap::VirtualArray>(P_CTRL);
  auto *p_env  = node.get_value_ref<hmap::VirtualArray>(P_ENV);
  auto *p_out  = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_out)
    return;

  // --- Params

  const auto gain   = node.val<float>(A_GAIN);
  const auto center = node.val<glm::vec2>(A_CENTER);

  // --- Compute

  hmap::for_each_tile(
      {p_out, p_dx, p_dy, p_ctrl},
      [&](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
      {
        auto [pa_out, pa_dx, pa_dy, pa_ctrl] = unpack<4>(p_arrays);

        *pa_out = hmap::bump(region.shape,
                             gain,
                             pa_ctrl,
                             pa_dx,
                             pa_dy,
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
