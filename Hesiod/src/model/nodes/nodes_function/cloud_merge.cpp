/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_CLOUD  = "cloud";
constexpr const char *P_CLOUD1 = "cloud1";
constexpr const char *P_CLOUD2 = "cloud2";

void setup_cloud_merge_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Cloud>(gnode::PortType::IN, P_CLOUD1);
  node.add_port<hmap::Cloud>(gnode::PortType::IN, P_CLOUD2);
  node.add_port<hmap::Cloud>(gnode::PortType::OUT, P_CLOUD);
}

void compute_cloud_merge_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Cloud *p_in1 = node.get_value_ref<hmap::Cloud>(P_CLOUD1);
  hmap::Cloud *p_in2 = node.get_value_ref<hmap::Cloud>(P_CLOUD2);

  if (p_in1 && p_in2)
  {
    hmap::Cloud *p_out = node.get_value_ref<hmap::Cloud>(P_CLOUD);

    // convert the input
    *p_out = hmap::merge_cloud(*p_in1, *p_in2);
  }
}

} // namespace hesiod
