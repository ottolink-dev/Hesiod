/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cmath>

#include <QEvent>
#include <QPainter>
#include <QPen>
#include <QRegion>
#include <QTimer>
#include <QVariantAnimation>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/build_border.hpp"

namespace hesiod
{

namespace
{
// a dash and a gap, in pen widths
constexpr qreal dash = 3.0;
constexpr qreal gap = 2.0;
} // namespace

BuildBorder::BuildBorder(QWidget *parent, std::function<QPainterPath()> outline)
    : QWidget(parent), outline(std::move(outline))
{
  this->setAttribute(Qt::WA_TransparentForMouseEvents);
  this->setAttribute(Qt::WA_NoSystemBackground);
  this->hide();
  parent->installEventFilter(this);

  // slowly, a dash and a gap in a second and a half
  this->motion = new QVariantAnimation(this);
  this->motion->setStartValue(0.0);
  this->motion->setEndValue(1.0);
  this->motion->setDuration(1500);
  this->motion->setLoopCount(-1);
  this->connect(this->motion,
                &QVariantAnimation::valueChanged,
                this,
                [this](const QVariant &value)
                {
                  // in steps: no need to repaint the line every frame
                  const qreal phase = std::floor(value.toReal() * 20.0) / 20.0;
                  if (phase == this->phase)
                    return;
                  this->phase = phase;
                  this->update_line();
                });
}

void BuildBorder::set_active(bool active)
{
  const AppSettings &settings = HSD_CTX.app_settings;
  active = active && settings.node_editor.show_build_border;

  if (active == this->isVisible())
    return;

  if (!active)
  {
    this->motion->stop();
    this->hide();
    return;
  }

  this->setGeometry(this->parentWidget()->rect());
  this->show();
  this->raise();

  if (settings.interface.enable_ui_animations)
    this->motion->start();
}

bool BuildBorder::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == this->parentWidget() && event->type() == QEvent::Resize)
  {
    this->setGeometry(this->parentWidget()->rect());
    // over what the parent raises when it lays out its children
    QTimer::singleShot(0, this, [this]() { this->raise(); });
  }
  return false;
}

void BuildBorder::paintEvent(QPaintEvent *)
{
  const QPainterPath path = this->outline();

  // a whole number of dashes around it, so that it has no seam (a little
  // longer or shorter to fit)
  const qreal period = (dash + gap) * width;
  const qreal stretch = path.length() / std::max(1., std::round(path.length() / period)) /
                        period;

  QColor color = HSD_CTX.app_settings.colors.building;
  color.setAlphaF(0.6);

  QPen pen(color, width);
  pen.setDashPattern({dash * stretch, gap * stretch});
  pen.setDashOffset(-this->phase * (dash + gap) * stretch);
  pen.setCapStyle(Qt::FlatCap);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(pen);
  painter.drawPath(path);
}

void BuildBorder::update_line()
{
  QRegion line;
  for (const QPolygonF &polygon : this->outline().toSubpathPolygons())
    for (qsizetype i = 1; i < polygon.size(); ++i)
      line += QRectF(polygon[i - 1], polygon[i])
                  .normalized()
                  .adjusted(-width, -width, width, width)
                  .toAlignedRect();
  this->update(line);
}

} // namespace hesiod
