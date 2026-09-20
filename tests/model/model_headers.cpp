/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <type_traits>
#include <utility>

#if __has_include(<QObject>) || __has_include("gnodegui/node_proxy.hpp")
#error "Compile model headers without Qt or GNodeGUI include paths"
#endif

#include "hesiod/model/graph/graph_node.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/port_catalog.hpp"

static_assert(
    std::is_same_v<decltype(std::declval<const hesiod::BaseNode &>().get_port_type(0)),
                   gnode::PortType>);
static_assert(std::is_same_v<decltype(std::declval<const hesiod::BaseNode &>()
                                          .get_port_type(std::string{})),
                             gnode::PortType>);
