/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
   Public License. The full license is in the file LICENSE, distributed with
   this software. */
#include "hesiod/gui/undo/undo_manager.hpp"

namespace hesiod
{

UndoManager::UndoManager() = default;

UndoManager &UndoManager::instance()
{
  static UndoManager inst;
  return inst;
}

QUndoStack *UndoManager::get_stack() { return &this->undo_stack; }

void UndoManager::clear() { this->undo_stack.clear(); }

} // namespace hesiod
