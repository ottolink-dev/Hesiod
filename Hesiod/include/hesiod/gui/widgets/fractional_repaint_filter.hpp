/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <cmath>

#include <QEvent>
#include <QObject>
#include <QWidget>

namespace hesiod
{

class FractionalRepaintFilter : public QObject
{
public:
  using QObject::QObject;

protected:
  bool eventFilter(QObject *object, QEvent *event) override
  {
    if (event->type() == QEvent::UpdateRequest)
      if (auto *widget = qobject_cast<QWidget *>(object); widget && widget->isWindow())
      {
        const qreal dpr = widget->devicePixelRatioF();
        if (!qFuzzyCompare(dpr, std::round(dpr)))
          // Partial backing-store updates can leave stale pixels or clip child
          // widgets at fractional scaling. Expand the window's dirty region
          // before Qt syncs it, including menus and other popup windows.
          // Doing this during Paint would be too late and risk a repaint loop.
          widget->update();
      }

    return false;
  }
};

} // namespace hesiod
