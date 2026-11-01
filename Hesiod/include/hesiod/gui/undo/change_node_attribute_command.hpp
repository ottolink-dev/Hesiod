/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
   Public License. The full license is in the file LICENSE, distributed with
   this software. */
#pragma once
#include <memory>
#include <string>

#include <QUndoCommand>
#include <nlohmann/json.hpp>

#include "hesiod/gui/undo/undo_manager.hpp"

namespace hesiod
{

class GraphNode;

// =====================================
// ChangeNodeAttributeCommand
// =====================================
class ChangeNodeAttributeCommand : public QUndoCommand
{
public:
  ChangeNodeAttributeCommand(std::weak_ptr<GraphNode> p_graph_node,
                             const std::string       &node_id,
                             const std::string       &container_name,
                             const nlohmann::json    &old_state,
                             const nlohmann::json    &new_state,
                             QUndoCommand            *parent = nullptr);

  ChangeNodeAttributeCommand(std::weak_ptr<GraphNode> p_graph_node,
                             const std::string       &node_id,
                             const std::string       &old_container_name,
                             const std::string       &new_container_name,
                             const nlohmann::json    &old_state,
                             const nlohmann::json    &new_state,
                             QUndoCommand            *parent = nullptr);

  void undo() override;
  void redo() override;
  int  id() const override;
  bool mergeWith(const QUndoCommand *other) override;

private:
  void apply(const std::string &container_name, const nlohmann::json &state);

  std::weak_ptr<GraphNode> p_graph_node;
  std::string              node_id;
  std::string              old_container_name;
  std::string              new_container_name;
  nlohmann::json           old_state;
  nlohmann::json           new_state;
  bool                     is_first_redo = true;
};

} // namespace hesiod
