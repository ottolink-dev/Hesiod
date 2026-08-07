/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/geometry/path.hpp"

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

constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "path";

constexpr const char *A_NPOINTS = "npoints";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_path_decimate_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Path>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::Path>(gnode::PortType::OUT, P_OUT);

  // attribute(s)
  node.add_attr<IntAttribute>(A_NPOINTS, "Point Count Target", 8, 2, 32);
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_path_decimate_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Path *p_in  = node.get_value_ref<hmap::Path>(P_IN);
  hmap::Path *p_out = node.get_value_ref<hmap::Path>(P_OUT);

  if (!p_in || p_in->size() < 2)
    return;

  // --- Params

  const auto npoints = node.get_attr<IntAttribute>(A_NPOINTS);

  // --- Compute

  *p_out = *p_in;
  *p_out = hmap::decimate_vw(*p_in, npoints);
}

} // namespace hesiod
