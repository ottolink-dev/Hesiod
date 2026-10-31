/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
   Public License. The full license is in the file LICENSE, distributed with
   this software. */
#pragma once
#include <QUndoCommand>
#include <QUndoGroup>
#include <QUndoStack>

namespace hesiod
{

// =====================================
// UndoCommandId
// =====================================
enum UndoCommandId : int
{
  UndoId_ChangeNodeAttribute = 1,
  UndoId_GraphTopology,
};

// =====================================
// UndoManager
// =====================================
class UndoManager
{
public:
  static UndoManager &instance();

  QUndoStack *get_stack();

  void clear();

private:
  UndoManager();
  ~UndoManager() = default;

  QUndoStack undo_stack;
};

} // namespace hesiod
