/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cmath>

#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

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

  this->fade = new QVariantAnimation(this);
  this->fade->setEasingCurve(QEasingCurve::OutCubic);
  QObject::connect(this->fade,
                   &QVariantAnimation::valueChanged,
                   this,
                   [this](const QVariant &v)
                   {
                     this->opacity = v.toReal();
                     this->update();
                   });
  QObject::connect(this->fade,
                   &QVariantAnimation::finished,
                   this,
                   [this]()
                   {
                     if (this->opacity <= 0.01)
                       this->hide();
                   });
}

void SnapGuide::set_target(const QRectF &rect)
{
  this->target = rect;
  this->update();
}

void SnapGuide::appear()
{
  this->setGeometry(this->parentWidget()->rect());
  this->show();
  this->raise();
  this->run_fade(1.0);
}

void SnapGuide::vanish() { this->run_fade(0.0); }

void SnapGuide::paintEvent(QPaintEvent *)
{
  if (this->target.isNull())
    return;

  const QColor accent = HSD_CTX.app_settings.colors.accent;
  QPainter     p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setOpacity(this->opacity);

  QColor fill = accent;
  fill.setAlphaF(0.16);
  QColor edge = accent;
  edge.setAlphaF(0.75);

  const qreal  r = std::min(10.0,
                           std::min(this->target.width(), this->target.height()) / 2);
  QPainterPath outline;
  outline.addRoundedRect(this->target.adjusted(0.75, 0.75, -0.75, -0.75), r, r);

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

void SnapGuide::run_fade(qreal to)
{
  this->fade->stop();
  this->fade->setStartValue(this->opacity);
  this->fade->setEndValue(to);
  this->fade->setDuration(anim_ms(140));
  this->fade->start();
}

} // namespace hesiod::rail
