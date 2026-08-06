/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/range.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"

using namespace attr;

namespace hesiod
{
// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_INPUT = "input";
constexpr const char *P_OUTPUT = "output";

constexpr const char *A_REMAP = "remap";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_remap_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_INPUT);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUTPUT, CONFIG(node));

  // attribute(s)
  node.add_attr<RangeAttribute>(A_REMAP, "remap");
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_remap_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto *p_in = node.get_value_ref<hmap::VirtualArray>(P_INPUT);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUTPUT);

  if (!p_in)
    return;

  hmap::copy_data(*p_in, *p_out, node.cfg().cm_cpu);

  p_out->remap(node.get_attr<RangeAttribute>(A_REMAP)[0],
               node.get_attr<RangeAttribute>(A_REMAP)[1],
               node.cfg().cm_cpu);
}

} // namespace hesiod
