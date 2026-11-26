/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include <QCursor>
#include <QMenu>
#include <QTimer>

#include "hesiod/gui/widgets/shell/popup_entrance.hpp"
#include "hesiod/gui/widgets/shell/shell_style.hpp"

namespace hesiod
{

namespace
{

// Where a menu or a list comes from, relative to its place: out of the side
// of the parent menu it belongs to, otherwise away from the pointer, so one
// that opens above the pointer rises and one that opens below it drops.
QPoint unfold_from(const QWidget *window)
{
  const QRect at = window->geometry();

  const auto *parent = qobject_cast<const QMenu *>(window->parentWidget());
  if (parent && parent->isVisible())
  {
    const int middle = parent->x() + parent->width() / 2;
    if (at.left() >= middle)
      return QPoint(-shell::popup_unfold, 0);
    if (at.right() < middle)
      return QPoint(shell::popup_unfold, 0);
    return QPoint(0, -shell::popup_unfold);
  }

  const bool above = at.bottom() < QCursor::pos().y() + shell::popup_pointer_margin;
  return QPoint(0, above ? shell::popup_unfold : -shell::popup_unfold);
}

} // namespace

PopupEntrance::PopupEntrance(QWidget *window_, Travel travel_, int duration_ms_)
    : QObject(window_), window(window_), travel(travel_), duration_ms(duration_ms_)
{
  // Show reaches the application's filters before the window's own showEvent,
  // and several of these windows only take their final place there (a dialog
  // centers itself, a menu is clamped to the screen). So: invisible now, and
  // the travel is worked out one turn of the event loop later.
  window_->setWindowOpacity(0.0);
  QTimer::singleShot(0, this, [this]() { this->start(); });
}

void PopupEntrance::cancel(QWidget *window)
{
  auto *entrance = window->findChild<PopupEntrance *>(QString(),
                                                      Qt::FindDirectChildrenOnly);
  if (!entrance)
    return;

  entrance->apply(1.0);
  delete entrance; // at once: the window may be shown again before the loop turns
}

void PopupEntrance::start()
{
  if (!this->window || !this->window->isVisible())
  {
    this->deleteLater();
    return;
  }

  switch (this->travel)
  {
  case Travel::None:
    break;
  case Travel::Rise:
    this->from = QPoint(0, shell::popup_rise);
    break;
  case Travel::Unfold:
    this->from = unfold_from(this->window);
    break;
  }

  this->progress.on_tick = [this]() { this->apply(this->progress.value()); };
  this->progress.on_done = [this]()
  {
    this->apply(1.0);
    this->deleteLater();
  };

  this->apply(0.0);
  this->progress.to(1.0, this->duration_ms, motion::enter());
}

void PopupEntrance::apply(double t)
{
  if (!this->window)
    return;

  t = std::clamp(t, 0.0, 1.0);

  // the fade runs ahead of the travel: the window is nearly solid while it
  // still has a little way to go, so the movement is seen, not just a blend
  this->window->setWindowOpacity(std::min(1.0, shell::popup_fade_lead * t));

  // Moved by the change since the last step, not to a remembered position: a
  // window that is placed again meanwhile (a tooltip following the pointer, a
  // dialog being dragged) keeps the place it was given.
  const QPoint off(qRound(this->from.x() * (1.0 - t)),
                   qRound(this->from.y() * (1.0 - t)));
  if (off != this->applied)
  {
    this->window->move(this->window->pos() + off - this->applied);
    this->applied = off;
  }
}

} // namespace hesiod
