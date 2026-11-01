/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
   Public License. The full license is in the file LICENSE, distributed with
   this software. */
#include "hesiod/gui/undo/graph_topology_commands.hpp"

#include <QObject>

#include "hesiod/gui/graph_editor.hpp"
#include "hesiod/gui/widgets/graph/graph_node_widget.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/graph/graph_node.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

// =====================================
// AddNodeCommand
// =====================================

AddNodeCommand::AddNodeCommand(GraphNodeWidget   *widget,
                               const std::string &node_type,
                               QPointF            position,
                               QUndoCommand      *parent)
    : QUndoCommand(parent), widget(widget), node_type(node_type), position(position)
{
  this->setText(QString("Add %1 Node").arg(QString::fromStdString(node_type)));
}

void AddNodeCommand::redo()
{
  if (!this->widget || !this->widget->get_editor())
    return;

  if (this->is_first_redo)
  {
    this->created_id = this->widget->get_editor()->add_node(this->node_type,
                                                            this->position);
    this->is_first_redo = false;
    return;
  }

  // Subsequent redo: recreate node and restore state if saved
  if (!this->saved_state.empty())
  {
    this->created_id = this->widget->get_editor()->add_node(
        this->node_type,
        this->position,
        [this](BaseNode &node) { node.json_from(this->saved_state); },
        this->created_id);
  }
  else
  {
    this->created_id = this->widget->get_editor()->add_node(this->node_type,
                                                            this->position,
                                                            {},
                                                            this->created_id);
  }
}

void AddNodeCommand::undo()
{
  if (!this->widget || !this->widget->get_editor() || this->created_id.empty())
    return;

  // Snapshot node state before erasing
  if (auto *gno = this->widget->get_p_graph_node())
  {
    if (auto *p_node = gno->get_node_ref_by_id<BaseNode>(this->created_id))
    {
      this->saved_state = p_node->json_to();
    }
  }

  this->widget->get_editor()->erase({this->created_id});
}

// =====================================
// EraseCommand
// =====================================

EraseCommand::EraseCommand(GraphNodeWidget                         *widget,
                           const std::vector<std::string>          &node_ids,
                           const std::vector<gngui::LinkEndpoints> &links,
                           QUndoCommand                            *parent)
    : QUndoCommand(parent), widget(widget), node_ids(node_ids), links(links)
{
  this->setText(QObject::tr("Delete Selection"));

  if (!this->widget || !this->widget->get_p_graph_node())
    return;

  auto *gno = this->widget->get_p_graph_node();
  auto *editor = this->widget->get_editor();

  // Save node states and incident links for all deleted nodes
  for (const auto &id : this->node_ids)
  {
    if (auto *p_node = gno->get_node_ref_by_id<BaseNode>(id))
    {
      QPointF pos;
      if (auto *gfx = this->widget->get_graphics_node_by_id(id))
        pos = gfx->pos();

      this->saved_nodes.push_back({id, p_node->get_label(), pos, p_node->json_to()});

      if (editor)
      {
        for (const auto &l : editor->get_links_for(id))
        {
          if (!hesiod::contains(this->saved_links, l))
            this->saved_links.push_back(l);
        }
      }
    }
  }

  // Also include explicitly deleted links
  for (const auto &l : this->links)
  {
    if (!hesiod::contains(this->saved_links, l))
      this->saved_links.push_back(l);
  }
}

void EraseCommand::redo()
{
  if (!this->widget || !this->widget->get_editor())
    return;

  this->widget->get_editor()->erase(this->node_ids, this->links);
  this->is_first_redo = false;
}

void EraseCommand::undo()
{
  if (!this->widget || !this->widget->get_editor())
    return;

  auto              *editor = this->widget->get_editor();
  GraphEditor::Batch batch(*editor);

  // Restore deleted nodes with their original IDs and state
  for (const auto &sn : this->saved_nodes)
  {
    editor->add_node(
        sn.type,
        sn.position,
        [&sn](BaseNode &node)
        {
          node.json_from(sn.state);
          node.set_id(sn.id);
        },
        sn.id);
  }

  // Restore deleted links
  for (const auto &link : this->saved_links)
  {
    editor->connect(link);
  }

  batch.commit();

  // Restore selection for deleted nodes so settings panel and viewers re-attach
  for (const auto &sn : this->saved_nodes)
  {
    this->widget->set_node_as_selected(sn.id);
  }
}

// =====================================
// ConnectCommand
// =====================================

ConnectCommand::ConnectCommand(GraphNodeWidget            *widget,
                               const gngui::LinkEndpoints &link,
                               QUndoCommand               *parent)
    : QUndoCommand(parent), widget(widget), link(link)
{
  this->setText(QObject::tr("Connect Nodes"));

  if (this->widget && this->widget->get_editor())
  {
    for (const auto &old : this->widget->get_editor()->get_links_for(link.node_in))
    {
      if (old.node_in == link.node_in && old.port_in == link.port_in)
        this->replaced_links.push_back(old);
    }
  }
}

void ConnectCommand::redo()
{
  if (!this->widget || !this->widget->get_editor())
    return;

  this->widget->get_editor()->connect(this->link);
  this->is_first_redo = false;
}

void ConnectCommand::undo()
{
  if (!this->widget || !this->widget->get_editor())
    return;

  auto              *editor = this->widget->get_editor();
  GraphEditor::Batch batch(*editor);

  editor->disconnect(this->link);

  for (const auto &old : this->replaced_links)
  {
    editor->connect(old);
  }

  batch.commit();
}

// =====================================
// MoveNodesCommand
// =====================================

MoveNodesCommand::MoveNodesCommand(GraphNodeWidget                *widget,
                                   const std::vector<std::string> &node_ids,
                                   const std::vector<QPointF>     &old_positions,
                                   const std::vector<QPointF>     &new_positions,
                                   QUndoCommand                   *parent)
    : QUndoCommand(parent), widget(widget), node_ids(node_ids),
      old_positions(old_positions), new_positions(new_positions)
{
  this->setText(QObject::tr("Move Nodes"));
}

int MoveNodesCommand::id() const { return UndoId_GraphTopology; }

bool MoveNodesCommand::mergeWith(const QUndoCommand *other)
{
  if (!other || other->id() != this->id())
    return false;

  const auto *cmd = static_cast<const MoveNodesCommand *>(other);
  if (cmd->widget != this->widget || cmd->node_ids != this->node_ids)
    return false;

  this->new_positions = cmd->new_positions;
  return true;
}

void MoveNodesCommand::undo()
{
  if (!this->widget)
    return;

  for (size_t i = 0; i < this->node_ids.size() && i < this->old_positions.size(); ++i)
  {
    if (auto *gfx = this->widget->get_graphics_node_by_id(this->node_ids[i]))
    {
      gfx->setPos(this->old_positions[i]);
    }
  }
}

void MoveNodesCommand::redo()
{
  if (this->is_first_redo)
  {
    this->is_first_redo = false;
    return;
  }

  if (!this->widget)
    return;

  for (size_t i = 0; i < this->node_ids.size() && i < this->new_positions.size(); ++i)
  {
    if (auto *gfx = this->widget->get_graphics_node_by_id(this->node_ids[i]))
    {
      gfx->setPos(this->new_positions[i]);
    }
  }
}

} // namespace hesiod
