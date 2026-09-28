/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

#include <QMouseEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QWidget>

#include "meta/core/attribute_container.hpp"
#include "meta_qt/ui/theme.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/gui/widgets/viewers/viewport/viewport_style.hpp"

namespace hesiod::viewport
{

// =====================================
// SunDome: drag the sun around the sky
// =====================================

// The sky hemisphere seen from straight above: the centre is the zenith, the
// rim the horizon, north up. Dragging places the sun; its distance from the
// centre is cos(elevation), its bearing the azimuth -- the same spherical
// mapping the renderer's light uses.
class SunDome final : public QWidget
{
public:
  SunDome(meta::Attribute<float> *azimuth,
          meta::Attribute<float> *elevation,
          QWidget                *parent)
      : QWidget(parent), azimuth(azimuth), elevation(elevation)
  {
    this->setObjectName("sunDome");
    this->setAttribute(Qt::WA_NoSystemBackground);
    this->setAutoFillBackground(false);
    this->setStyleSheet("QWidget#sunDome { background: transparent; }");
    this->setFixedHeight(176);
    this->setCursor(Qt::CrossCursor);
    this->setToolTip("Drag to move the sun. Centre: overhead, rim: horizon.");

    if (azimuth)
      this->connections.push_back(
          azimuth->value_changed.subscribe([this](const float &) { this->update(); }));
    if (elevation)
      this->connections.push_back(
          elevation->value_changed.subscribe([this](const float &) { this->update(); }));
  }

protected:
  void paintEvent(QPaintEvent *) override
  {
    const auto &colors = HSD_CTX.app_settings.colors;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QPointF c = QRectF(this->rect()).center();
    const qreal   R = this->radius();

    // sky: lighter towards the sun, darker at the rim
    const QPointF   sun = this->sun_position();
    QRadialGradient sky(sun, R * 1.6, sun);
    sky.setColorAt(0.0, QColor("#8fa3b8"));
    sky.setColorAt(0.45, QColor("#5d6d80"));
    sky.setColorAt(1.0, QColor("#2c333d"));
    p.setPen(QPen(mix_colors(colors.bg_primary, colors.text_primary, 0.55), 1.5));
    p.setBrush(sky);
    p.drawEllipse(c, R, R);

    // elevation rings at 30 and 60 degrees, and the compass cross
    p.setBrush(Qt::NoBrush);
    QColor faint = colors.text_primary;
    faint.setAlphaF(0.10);
    p.setPen(QPen(faint, 1, Qt::DashLine));
    for (const qreal el : {30.0, 60.0})
    {
      const qreal r = std::cos(el * kPi / 180.0) * R;
      p.drawEllipse(c, r, r);
    }
    p.drawLine(c + QPointF(-R, 0), c + QPointF(R, 0));
    p.drawLine(c + QPointF(0, -R), c + QPointF(0, R));

    QColor letters = colors.text_primary;
    letters.setAlphaF(0.45);
    p.setPen(letters);
    p.setFont(meta::qt::ui_font(10, true));
    p.drawText(QRectF(c.x() - 8, c.y() - R + 3, 16, 14), Qt::AlignCenter, "N");
    p.drawText(QRectF(c.x() - 8, c.y() + R - 17, 16, 14), Qt::AlignCenter, "S");
    p.drawText(QRectF(c.x() + R - 17, c.y() - 7, 14, 14), Qt::AlignCenter, "E");
    p.drawText(QRectF(c.x() - R + 3, c.y() - 7, 14, 14), Qt::AlignCenter, "W");

    // the sun: glow, then disc
    QRadialGradient glow(sun, 26);
    glow.setColorAt(0.0, QColor(255, 244, 214, 200));
    glow.setColorAt(1.0, QColor(255, 244, 214, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(glow);
    p.drawEllipse(sun, 26, 26);
    p.setBrush(QColor("#fff6e0"));
    p.setPen(QPen(QColor(255, 255, 255, 220), 1.5));
    p.drawEllipse(sun, 11, 11);
  }

  void mousePressEvent(QMouseEvent *event) override { this->drag_to(event->position()); }

  void mouseMoveEvent(QMouseEvent *event) override
  {
    if (event->buttons() & Qt::LeftButton)
      this->drag_to(event->position());
  }

private:
  qreal radius() const { return std::min(this->width(), this->height()) / 2.0 - 8.0; }

  QPointF sun_position() const
  {
    const QPointF c = QRectF(this->rect()).center();
    const qreal   R = this->radius();
    const qreal   az = (this->azimuth ? this->azimuth->value() : 0.f) * kPi / 180.0;
    const qreal   el = (this->elevation ? this->elevation->value() : 45.f) * kPi / 180.0;
    const qreal   r = std::cos(el) * R;
    // renderer: x = cos(el) sin(az), z = cos(el) cos(az); z points at the
    // viewer, which is down on this top view
    return c + QPointF(std::sin(az) * r, std::cos(az) * r);
  }

  void drag_to(const QPointF &pos)
  {
    const QPointF c = QRectF(this->rect()).center();
    const QPointF d = (pos - c) / this->radius();
    const qreal   r = std::min(1.0, std::hypot(d.x(), d.y()));
    const qreal   el = std::acos(r) * 180.0 / kPi;
    const qreal   az = std::atan2(d.x(), d.y()) * 180.0 / kPi;

    if (this->azimuth && r > 1e-3)
      this->azimuth->set_value(float(az));
    if (this->elevation)
      this->elevation->set_value(float(el));
    this->update();
  }

  meta::Attribute<float>            *azimuth = nullptr;
  meta::Attribute<float>            *elevation = nullptr;
  std::vector<meta::EventConnection> connections;
};

} // namespace hesiod::viewport
