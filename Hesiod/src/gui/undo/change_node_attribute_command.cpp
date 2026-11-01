/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
   Public License. The full license is in the file LICENSE, distributed with
   this software. */
#include "hesiod/gui/undo/change_node_attribute_command.hpp"
#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/model/graph/graph_node.hpp"
#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

ChangeNodeAttributeCommand::ChangeNodeAttributeCommand(
    std::weak_ptr<GraphNode> p_graph_node,
    const std::string       &node_id,
    const std::string       &container_name,
    const nlohmann::json    &old_state,
    const nlohmann::json    &new_state,
    QUndoCommand            *parent)
    : ChangeNodeAttributeCommand(p_graph_node,
                                 node_id,
                                 container_name,
                                 container_name,
                                 old_state,
                                 new_state,
                                 parent)
{
}

ChangeNodeAttributeCommand::ChangeNodeAttributeCommand(
    std::weak_ptr<GraphNode> p_graph_node,
    const std::string       &node_id,
    const std::string       &old_container_name,
    const std::string       &new_container_name,
    const nlohmann::json    &old_state,
    const nlohmann::json    &new_state,
    QUndoCommand            *parent)
    : QUndoCommand(parent), p_graph_node(p_graph_node), node_id(node_id),
      old_container_name(old_container_name), new_container_name(new_container_name),
      old_state(old_state), new_state(new_state)
{
  if (old_container_name != new_container_name && !old_container_name.empty() &&
      !new_container_name.empty())
  {
    this->setText(QString("Switch %1 Category to %2")
                      .arg(QString::fromStdString(node_id))
                      .arg(QString::fromStdString(new_container_name)));
  }
  else
  {
    this->setText(QString("Change %1 Settings").arg(QString::fromStdString(node_id)));
  }
}

int ChangeNodeAttributeCommand::id() const { return UndoId_ChangeNodeAttribute; }

bool ChangeNodeAttributeCommand::mergeWith(const QUndoCommand *other)
{
  if (!HSD_CTX.app_settings.node_editor.merge_node_attribute_undo)
    return false;

  if (!other || other->id() != this->id())
    return false;

  const auto *cmd = static_cast<const ChangeNodeAttributeCommand *>(other);
  if (cmd->node_id != this->node_id)
    return false;

  if (cmd->old_container_name != this->old_container_name ||
      cmd->new_container_name != this->new_container_name)
    return false;

  if (this->p_graph_node.lock() != cmd->p_graph_node.lock())
    return false;

  this->new_state = cmd->new_state;
  return true;
}

void ChangeNodeAttributeCommand::undo()
{
  this->apply(this->old_container_name, this->old_state);
}

void ChangeNodeAttributeCommand::redo()
{
  if (this->is_first_redo)
  {
    this->is_first_redo = false;
    return;
  }

  this->apply(this->new_container_name, this->new_state);
}

void ChangeNodeAttributeCommand::apply(const std::string    &target_container_name,
                                       const nlohmann::json &state)
{
  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  BaseNode *p_node = gno->get_node_ref_by_id<BaseNode>(this->node_id);
  if (!p_node)
    return;

  auto &group = p_node->get_meta_group();

  if (!target_container_name.empty() && group.contains(target_container_name))
  {
    group.set_current(target_container_name);
  }

  if (!state.empty())
  {
    meta::AttributeContainer *container = nullptr;

    if (target_container_name.empty())
      container = &group.current();
    else
      container = group.find(target_container_name);

    if (container)
      container->json_from(state, meta::SerializationMode::state, true);
  }

  p_node->post_update_event.notify(*p_node);
  gno->update(this->node_id);
}

} // namespace hesiod
