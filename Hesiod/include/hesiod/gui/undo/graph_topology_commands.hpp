/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
   Public License. The full license is in the file LICENSE, distributed with
   this software. */
#pragma once
#include <memory>
#include <string>
#include <vector>

#include <QPointF>
#include <QUndoCommand>
#include <nlohmann/json.hpp>

#include "gnodegui/graph_viewer.hpp"
#include "hesiod/gui/undo/undo_manager.hpp"

namespace hesiod
{

class GraphEditor;
class GraphNodeWidget;

// =====================================
// AddNodeCommand
// =====================================
class AddNodeCommand : public QUndoCommand
{
public:
  AddNodeCommand(GraphNodeWidget   *widget,
                 const std::string &node_type,
                 QPointF            position,
                 QUndoCommand      *parent = nullptr);

  void        undo() override;
  void        redo() override;
  std::string get_created_node_id() const { return this->created_id; }

private:
  GraphNodeWidget *widget = nullptr;
  std::string      node_type;
  QPointF          position;
  std::string      created_id;
  nlohmann::json   saved_state;
  bool             is_first_redo = true;
};

// =====================================
// EraseCommand
// =====================================
class EraseCommand : public QUndoCommand
{
public:
  EraseCommand(GraphNodeWidget                         *widget,
               const std::vector<std::string>          &node_ids,
               const std::vector<gngui::LinkEndpoints> &links,
               QUndoCommand                            *parent = nullptr);

  void undo() override;
  void redo() override;

private:
  GraphNodeWidget                  *widget = nullptr;
  std::vector<std::string>          node_ids;
  std::vector<gngui::LinkEndpoints> links;

  struct NodeSnapshot
  {
    std::string    id;
    std::string    type;
    QPointF        position;
    nlohmann::json state;
  };
  std::vector<NodeSnapshot>         saved_nodes;
  std::vector<gngui::LinkEndpoints> saved_links;
  bool                              is_first_redo = true;
};

// =====================================
// ConnectCommand
// =====================================
class ConnectCommand : public QUndoCommand
{
public:
  ConnectCommand(GraphNodeWidget            *widget,
                 const gngui::LinkEndpoints &link,
                 QUndoCommand               *parent = nullptr);

  void undo() override;
  void redo() override;

private:
  GraphNodeWidget                  *widget = nullptr;
  gngui::LinkEndpoints              link;
  std::vector<gngui::LinkEndpoints> replaced_links;
  bool                              is_first_redo = true;
};

// =====================================
// MoveNodesCommand
// =====================================
class MoveNodesCommand : public QUndoCommand
{
public:
  MoveNodesCommand(GraphNodeWidget                *widget,
                   const std::vector<std::string> &node_ids,
                   const std::vector<QPointF>     &old_positions,
                   const std::vector<QPointF>     &new_positions,
                   QUndoCommand                   *parent = nullptr);

  void undo() override;
  void redo() override;
  int  id() const override;
  bool mergeWith(const QUndoCommand *other) override;

private:
  GraphNodeWidget         *widget = nullptr;
  std::vector<std::string> node_ids;
  std::vector<QPointF>     old_positions;
  std::vector<QPointF>     new_positions;
  bool                     is_first_redo = true;
};

} // namespace hesiod
