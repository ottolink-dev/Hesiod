/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <string>

#include <QApplication>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QWidget>

#include "logger/logger.hpp"

namespace hesiod
{

void render_widget_screenshot(QWidget              *widget,
                              const std::string    &fname,
                              const QSize          &size,
                              std::function<void()> post_render_callback)
{
  LOG_TRACE("render_widget_screenshot: fname {}", fname);

  // If a target size is provided, fix the widget to that size
  if (size.isValid())
  {
    widget->setFixedSize(size);
  }
  else
  {
    // Let layout decide but ensure geometry is updated
    widget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    widget->adjustSize();
  }

  // ensure all pending events/layout updates are processed
  QApplication::processEvents();

  // create a QPixmap or QImage for off-screen rendering
  QSize  render_size = size.isValid() ? size : widget->size();
  QImage image(render_size, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);

  QPainter painter(&image);
  widget->render(&painter);
  painter.end();

  // can be used to setup some size-dependent stuff...
  if (post_render_callback)
    post_render_callback();

  if (size.isValid())
  {
    image.save(QString::fromStdString(fname));
  }
  else
  {
    QRect  crop_rect(QPoint(0, 0), widget->sizeHint().boundedTo(render_size));
    QImage cropped = image.copy(crop_rect);
    cropped.save(QString::fromStdString(fname));
  }
}

} // namespace hesiod
