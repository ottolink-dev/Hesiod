/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/range.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "output";

constexpr const char *A_VMAX             = "vmax";
constexpr const char *A_ANGLE            = "angle";
constexpr const char *A_SLOPE            = "slope";
constexpr const char *A_CENTER           = "center";
constexpr const char *A_USE_MAX_OPERATOR = "use_max_operator";
constexpr const char *A_K                = "k";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_clamp_oblique_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  node.set_current_category("Clamp Parameters");
  add_float(node, A_VMAX, "Clamp Value", 0.5f, -FLT_MAX, FLT_MAX);
  add_angle(node, A_ANGLE, "Angle", 0.f, -180.f, 180.f);
  add_float(node, A_SLOPE, "Slope", 0.2f, 0.f, 4.f);
  add_xy(node, A_CENTER, "Center");
  add_bool(node, A_USE_MAX_OPERATOR, "Clamp Mode", "Max", "Min", true);

  node.set_current_category("Smoothing");
  add_float(node, A_K, "Smoothing", 0.f, 0.f, 1.f);

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_clamp_oblique_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in  = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in || !p_out)
    return;

  // --- Params

  const auto vmax             = node.val<float>(A_VMAX);
  const auto angle            = node.val<float>(A_ANGLE);
  const auto slope            = node.val<float>(A_SLOPE);
  const auto use_max_operator = node.val<bool>(A_USE_MAX_OPERATOR);
  const auto k                = node.val<float>(A_K);
  const auto center           = node.val<glm::vec2>(A_CENTER);

  // --- Compute

  hmap::for_each_tile(
      {p_out, p_in},
      [&](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
      {
        auto [pa_out, pa_in] = unpack<2>(p_arrays);
        *pa_out = *pa_in;

        hmap::clamp_oblique_plane(*pa_out,
                                  vmax,
                                  angle,
                                  slope,
                                  use_max_operator,
                                  k,
                                  center,
                                  region.bbox);
      },
      node.cfg().cm_cpu);

  // --- Post-process

  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
