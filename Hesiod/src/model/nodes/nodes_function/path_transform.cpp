/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/geometry/path.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "output";

// Group: Scale
constexpr const char *G_SCALE  = "Scale";
constexpr const char *A_SCALE  = "scale";
constexpr const char *A_CENTER = "center";

// Group: Inflate
constexpr const char *G_INFLATE           = "Inflate";
constexpr const char *A_RADIUS            = "radius";
constexpr const char *A_ENABLE_RESAMPLING = "enable_resampling";
constexpr const char *A_RESAMPLE_DELTA    = "resample_delta";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_path_transform_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Path>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::Path>(gnode::PortType::OUT, P_OUT);

  // Group: Scale
  {
    node.set_current_group(G_SCALE);

    add_float(node, A_SCALE, "Scale", 1.f, 0.01f, 1.5f);
    add_xy(node, A_CENTER, "Center", {0.5f, 0.5f}, 0.f, 1.f, 0.f, 1.f);
  }

  // Group: Inflate
  {
    node.set_current_group(G_INFLATE);

    // clang-format off
    add_float(node, A_RADIUS, "Inflation Radius", 0.f, -0.1f, 0.5f);
    add_bool(node, A_ENABLE_RESAMPLING, "Enable Resampling", true);
    add_float(node, A_RESAMPLE_DELTA, "Step Size", 0.01f, 0.0001f, 0.1f, "{:.2e}", true);
    // clang-format on
  }

  // Reset active group to first
  node.set_current_group(G_SCALE);
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_path_transform_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Path *p_in = node.get_value_ref<hmap::Path>(P_IN);

  if (!p_in || p_in->empty())
    return;

  hmap::Path *p_out = node.get_value_ref<hmap::Path>(P_OUT);

  const std::string
      current_group = node.get_meta_group().current_container_name().value_or(G_SCALE);

  Logger::log()->trace("compute_path_transform_node: current_group {}", current_group);

  if (current_group == G_SCALE)
  {
    const auto scale  = node.val<float>(A_SCALE);
    const auto center = node.val<glm::vec2>(A_CENTER);

    *p_out = hmap::scale(*p_in, scale, center);
  }
  else if (current_group == G_INFLATE)
  {
    if (p_in->size() < 3)
      return;

    const auto radius            = node.val<float>(A_RADIUS);
    const auto enable_resampling = node.val<bool>(A_ENABLE_RESAMPLING);
    const auto delta             = node.val<float>(A_RESAMPLE_DELTA);

    const auto npoints = std::max(3, int(p_in->get_arc_length().back() / delta));

    *p_out = *p_in;

    if (enable_resampling)
      p_out->resample_interp(npoints);

    *p_out = hmap::inflate(*p_out, radius);
  }
  else
  {
    Logger::log()->error("compute_path_transform_node: group {} not implemented",
                         current_group);
  }
}

} // namespace hesiod
