/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <cmath>

#include <QElapsedTimer>
#include <QEvent>
#include <QObject>
#include <QTimer>
#include <QWidget>

namespace hesiod
{

// Partial backing-store updates can leave stale pixels or clip child widgets
// at fractional scaling (see docs/reviews/pr-758-linux.md). The fix is to
// expand a window's dirty region to the whole window before Qt syncs it.
//
// Doing that on *every* update is correct but expensive: a continuously
// animated child (the OpenGL viewport, a drag, a live resize) then repaints the
// entire window in software on every frame, which caps the viewport at ~25 fps
// on a 4K display. So the expansion adapts to the update rate:
//
// - isolated updates (a hover change, a click) are expanded immediately, as
//   before, so what the user looks at is always clean;
// - while a window is streaming updates, they stay partial, and one full
//   repaint follows as soon as the stream stops, wiping anything left behind.
//
// Popups (menus, tooltips, flyouts) are small and always fully repainted.
class FractionalRepaintFilter : public QObject
{
public:
  explicit FractionalRepaintFilter(QObject *parent = nullptr) : QObject(parent)
  {
    this->clock.start();
  }

  // two requests closer than this count as a stream
  static constexpr qint64 stream_gap_ms = 50;
  // full repaint once a stream has been quiet for this long
  static constexpr int settle_ms = 120;

protected:
  bool eventFilter(QObject *object, QEvent *event) override
  {
    if (event->type() != QEvent::UpdateRequest)
      return false;

    auto *widget = qobject_cast<QWidget *>(object);
    if (!widget || !widget->isWindow())
      return false;

    const qreal dpr = widget->devicePixelRatioF();
    if (qFuzzyCompare(dpr, std::round(dpr)))
      return false;

    // Doing this during Paint would be too late and risk a repaint loop.
    const Qt::WindowType type = widget->windowType();
    if (type == Qt::Popup || type == Qt::ToolTip)
    {
      widget->update();
      return false;
    }

    const qint64 now = this->clock.elapsed();
    const qint64 last = widget->property("_hsd_last_update_request").toLongLong();
    widget->setProperty("_hsd_last_update_request", now);

    if (last == 0 || now - last > stream_gap_ms)
    {
      widget->update(); // isolated update: clean right away
      return false;
    }

    // streaming: stay partial, (re)arm the settle repaint
    auto *settle = widget->findChild<QTimer *>("_hsd_settle_repaint",
                                               Qt::FindDirectChildrenOnly);
    if (!settle)
    {
      settle = new QTimer(widget);
      settle->setObjectName("_hsd_settle_repaint");
      settle->setSingleShot(true);
      settle->setInterval(settle_ms);
      QObject::connect(settle,
                       &QTimer::timeout,
                       widget,
                       [widget]() { widget->update(); });
    }
    settle->start();

    return false;
  }

private:
  QElapsedTimer clock;
};

} // namespace hesiod
