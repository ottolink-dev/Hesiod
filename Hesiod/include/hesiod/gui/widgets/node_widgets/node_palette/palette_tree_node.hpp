/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <memory>
#include <vector>

#include <QColor>
#include <QString>

namespace hesiod
{

/// One category of the node library, or one of its sub-categories: what a
/// flyout card lists.
struct PaletteTreeNode
{
  QString                                       title;
  QString                                       path;     // "Erosion/Hydraulic"
  QString                                       category; // the top-level one, "Erosion"
  QColor                                        accent;
  std::vector<QString>                          leaves; // node types, sorted
  std::vector<std::shared_ptr<PaletteTreeNode>> children;

  int total_leaves() const
  {
    int n = static_cast<int>(this->leaves.size());
    for (const auto &child : this->children)
      if (child)
        n += child->total_leaves();
    return n;
  }
};

} // namespace hesiod
