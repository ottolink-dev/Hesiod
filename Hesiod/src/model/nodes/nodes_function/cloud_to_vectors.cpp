/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/geometry/cloud.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/node_factory.hpp"
#include "hesiod/model/nodes/post_process.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_CLOUD = "cloud";

void setup_cloud_to_vectors_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Cloud>(gnode::PortType::IN, P_CLOUD);
  node.add_port<std::vector<float>>(gnode::PortType::OUT, "x");
  node.add_port<std::vector<float>>(gnode::PortType::OUT, "y");
  node.add_port<std::vector<float>>(gnode::PortType::OUT, "v");

  // attribute(s)
}

void compute_cloud_to_vectors_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Cloud *p_in = node.get_value_ref<hmap::Cloud>(P_CLOUD);

  if (p_in)
  {
    auto p_x = node.get_value_ref<std::vector<float>>("x");
    auto p_y = node.get_value_ref<std::vector<float>>("y");
    auto p_v = node.get_value_ref<std::vector<float>>("v");

    *p_x = p_in->get_x();
    *p_y = p_in->get_y();
    *p_v = p_in->get_values();
  }
}

} // namespace hesiod
