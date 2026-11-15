/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QLabel>

#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/utils.hpp"
#include "logger/logger.hpp"

namespace hesiod
{

void setup_debug_node(BaseNode &node)
{
  LOG_TRACE("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");

  // no attribute(s)
}

void compute_debug_node(BaseNode &node)
{
  LOG_TRACE("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // nothing here on purpose, everything's made in the NodeWidget
}

} // namespace hesiod
