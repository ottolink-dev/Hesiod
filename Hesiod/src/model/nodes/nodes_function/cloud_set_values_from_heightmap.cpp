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
constexpr const char *P_CLOUD     = "cloud";
constexpr const char *P_HEIGHTMAP = "heightmap";
constexpr const char *P_OUT       = "output";

void setup_cloud_set_values_from_heightmap_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Cloud>(gnode::PortType::IN, P_CLOUD);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_HEIGHTMAP);
  node.add_port<hmap::Cloud>(gnode::PortType::OUT, P_OUT);

  // attribute(s)
}

void compute_cloud_set_values_from_heightmap_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Cloud        *p_cloud = node.get_value_ref<hmap::Cloud>(P_CLOUD);
  hmap::VirtualArray *p_hmap  = node.get_value_ref<hmap::VirtualArray>(P_HEIGHTMAP);

  if (p_cloud && p_hmap)
  {
    hmap::Cloud *p_out = node.get_value_ref<hmap::Cloud>(P_OUT);

    // TODO distribute
    hmap::Array array = p_hmap->to_array(node.cfg().cm_cpu);

    *p_out = *p_cloud;
    p_out->set_values_from_array(array);
  }
}

} // namespace hesiod
