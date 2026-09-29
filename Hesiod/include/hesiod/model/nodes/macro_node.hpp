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

/// One port of a macro, coming from a MacroInput* / MacroOutput* body node.
struct MacroPort
{
  std::string name;      // port label on the instance
  std::string node_id;   // interface node in the body
  std::string data_type; // typeid name
  bool        is_input = true;
};

bool is_macro_io_node_type(const std::string &node_type);
bool is_macro_body(const GraphNode &graph);

// typeid name of the data a MacroInput* / MacroOutput* node type carries
std::string macro_io_data_type(const std::string &node_type);

// node types to offer in a graph: MacroInput* / MacroOutput* only in a macro body
std::map<std::string, std::string> node_inventory_for(const GraphNode &graph);

// ports and settings of the MacroInput* / MacroOutput* node types
void setup_macro_io_node(BaseNode &node);

// =====================================
// MacroNode
// =====================================
class MacroNode : public BaseNode
{
public:
  MacroNode(const std::string &label, std::weak_ptr<GraphConfig> config);

  /**
   * @brief Build the body and the ports from a definition.
   *
   * Ports can only be created once, so this is for a new node. A definition
   * with other ports replaces the node instead, see rebuild_macro_node().
   * Throws when the definition contains an instance of itself.
   */
  void set_definition(const nlohmann::json &definition);

  /// Same as set_definition() with a body that already exists (the one open in
  /// the editor, kept when its ports change).
  void set_body(std::shared_ptr<GraphNode> new_body, const nlohmann::json &definition);

  /// The definition, with the body as it is now.
  nlohmann::json get_definition() const;

  std::string                   get_macro_id() const;
  std::string                   get_macro_name() const;
  int                           get_revision() const;
  GraphNode                    *get_body() const { return this->body.get(); }
  std::shared_ptr<GraphNode>    get_body_shared() const { return this->body; }
  const std::vector<MacroPort> &get_macro_ports() const { return this->ports_info; }
  bool                          is_computing() const { return this->computing; }

  /// The editor computed the body itself: the next compute only publishes
  /// the outputs instead of computing the body again.
  void mark_body_computed() { this->body_computed = true; }

  /// While the body is open in an editor, its layout comes from there.
  std::function<nlohmann::json()> layout_provider;

  // --- BaseNode ---
  void           json_from(nlohmann::json const &json) override;
  nlohmann::json json_to() const override;
  void           propagate_config_change() override;

  void compute_macro();

private:
  void move_data(bool inputs); // into the body, or out of it

  nlohmann::json             definition; // everything but the body graph
  std::shared_ptr<GraphNode> body;
  std::vector<MacroPort>     ports_info;
  bool                       body_computed = false;
  bool                       body_stale = true;   // next compute runs the whole body
  bool                       computing = false;
};

} // namespace hesiod
