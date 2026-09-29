/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */

/**
 * @file macro_node.hpp
 * @brief Macros: a group of nodes packed into one node with its own ports.
 *
 * A macro instance embeds its whole definition (the body graph, the editor
 * layout of that body, a name and an id shared by linked instances), so a
 * project opens the same everywhere and copy/paste carries macros along. The
 * body is a regular GraphNode owned by the instance. Its MacroInput* and
 * MacroOutput* nodes are the instance's ports.
 *
 * Definition layout:
 *   { "format": 1, "macro_id": "...", "name": "...", "revision": 0,
 *     "graph": <GraphNode::json_to()>, "layout": <GraphViewer::json_to()> }
 */
#pragma once
#include <functional>
#include <memory>

#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

class GraphNode; // forward

bool is_macro_io_node_type(const std::string &node_type);
bool is_macro_body(const GraphNode &graph);

// typeid name of the data a MacroInput* / MacroOutput* node type carries
std::string macro_io_data_type(const std::string &node_type);

// node types to offer in a graph: MacroInput* / MacroOutput* only in a macro body
std::map<std::string, std::string> node_inventory_for(const GraphNode &graph);

// ports and settings of the MacroInput* / MacroOutput* node types
void setup_macro_io_node(BaseNode &node);

} // namespace hesiod
