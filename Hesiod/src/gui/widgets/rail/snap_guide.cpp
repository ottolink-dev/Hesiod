/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cmath>

#include <QPainter>
#include <QPainterPath>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/rail/rail_style.hpp"
#include "hesiod/gui/widgets/rail/snap_guide.hpp"

namespace hesiod::rail
{

SnapGuide::SnapGuide(QWidget *parent) : QWidget(parent)
{
  this->setAttribute(Qt::WA_TransparentForMouseEvents);
  this->setAttribute(Qt::WA_NoSystemBackground);
  this->hide();

  this->fade.on_done = [this]()
  {
    if (this->fade.value() <= 0.01)
    {
      this->has_target = false;
      this->hide();
    }
  };
}

QRectF SnapGuide::current_rect() const
{
  const qreal t = std::clamp(this->glide.value(), 0.0, 1.0);
  return QRectF(
      this->from_rect.x() + (this->to_rect.x() - this->from_rect.x()) * t,
      this->from_rect.y() + (this->to_rect.y() - this->from_rect.y()) * t,
      this->from_rect.width() + (this->to_rect.width() - this->from_rect.width()) * t,
      this->from_rect.height() + (this->to_rect.height() - this->from_rect.height()) * t);
}

void SnapGuide::set_target(const QRectF &rect)
{
  if (!this->has_target)
  {
    this->from_rect = rect;
    this->to_rect = rect;
    this->has_target = true;
    this->glide.set(1.0);
    this->update();
    return;
  }

  if (this->to_rect == rect)
    return;

  this->from_rect = this->current_rect();
  this->to_rect = rect;
  this->glide.set(0.0);
  this->glide.to(1.0, motion::snap_glide, motion::enter());
}

void SnapGuide::appear()
{
  this->setGeometry(this->parentWidget()->rect());
  this->show();
  this->raise();
  this->fade.to(1.0, motion::snap_in, motion::enter());
}

void SnapGuide::vanish() { this->fade.to(0.0, motion::snap_out, motion::leave()); }

void SnapGuide::paintEvent(QPaintEvent *)
{
  if (!this->has_target)
    return;

  const QRectF box = this->current_rect();
  if (box.isNull())
    return;

  const QColor accent = HSD_CTX.app_settings.colors.accent;
  QPainter     p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setOpacity(std::clamp(this->fade.value(), 0.0, 1.0));

  QColor fill = accent;
  fill.setAlphaF(0.16);
  QColor edge = accent;
  edge.setAlphaF(0.75);

  const qreal  r = std::min(10.0, std::min(box.width(), box.height()) / 2.0);
  QPainterPath outline;
  outline.addRoundedRect(box.adjusted(0.75, 0.75, -0.75, -0.75), r, r);

  // a whole number of dashes around it (those of Qt::DashLine, a little longer
  // or shorter to fit): otherwise one is cut where the outline starts
  const qreal width = 1.5;
  const qreal period = 6.0 * width;
  const qreal stretch = outline.length() /
                        std::max(1.0, std::round(outline.length() / period)) / period;

  QPen pen(edge, width);
  pen.setDashPattern({4.0 * stretch, 2.0 * stretch});
  pen.setCapStyle(Qt::FlatCap);

  p.setPen(pen);
  p.setBrush(fill);
  p.drawPath(outline);
}

} // namespace hesiod::rail
