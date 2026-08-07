/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/geometry/path.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "path";

constexpr const char *A_RADIUS            = "radius";
constexpr const char *A_ENABLE_RESAMPLING = "enable_resampling";
constexpr const char *A_RESAMPLE_DELTA    = "resample_delta";

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_path_inflate_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Path>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::Path>(gnode::PortType::OUT, P_OUT);

  // attribute(s)
  // clang-format off
  add_float(node, A_RADIUS, "Inflation Radius", 0.f, -0.1f, 0.5f);
  add_bool(node, A_ENABLE_RESAMPLING, "Enable Resampling", true);
  add_float(node, A_RESAMPLE_DELTA, "Step Size", 0.01f, 0.0001f, 0.1f, "{:.2e}", true);
  // clang-format on
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_path_inflate_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Path *p_in  = node.get_value_ref<hmap::Path>(P_IN);
  hmap::Path *p_out = node.get_value_ref<hmap::Path>(P_OUT);

  if (!p_in || p_in->size() < 3)
    return;

  // --- Params

  const auto radius            = node.val<float>(A_RADIUS);
  const auto enable_resampling = node.val<bool>(A_ENABLE_RESAMPLING);
  const auto delta             = node.val<float>(A_RESAMPLE_DELTA);
  //
  const auto npoints = std::max(3, int(p_in->get_arc_length().back() / delta));

  // --- Compute

  *p_out = *p_in;

  if (enable_resampling)
    p_out->resample_interp(npoints);

  *p_out = hmap::inflate(*p_out, radius);
}

} // namespace hesiod
