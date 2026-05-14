/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QApplication>
#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>

#include "hesiod/gui/widgets/node_library_widget.hpp"
#include "hesiod/logger.hpp"

namespace hesiod
{

LibraryTreeWidget::LibraryTreeWidget(QWidget *parent) : QTreeWidget(parent)
{
  this->setDragEnabled(true);
}

void LibraryTreeWidget::mousePressEvent(QMouseEvent *e)
{
  if (e->button() == Qt::LeftButton)
    this->drag_start_pos = e->pos();
  QTreeWidget::mousePressEvent(e);
}

void LibraryTreeWidget::mouseMoveEvent(QMouseEvent *e)
{
  if (!(e->buttons() & Qt::LeftButton))
    return;
  if ((e->pos() - this->drag_start_pos).manhattanLength() <
      QApplication::startDragDistance())
    return;

  QTreeWidgetItem *item = itemAt(drag_start_pos);
  if (!item || item->childCount() > 0) // only leaves
    return;

  std::string node_type = item->data(0, Qt::UserRole).toString().toStdString();

  QMimeData *mime = new QMimeData();
  mime->setText(QString::fromStdString(node_type));

  QDrag *drag = new QDrag(this);
  drag->setMimeData(mime);
  drag->exec(Qt::CopyAction);

  Q_EMIT this->node_type_dragged(node_type);
}

} // namespace hesiod
