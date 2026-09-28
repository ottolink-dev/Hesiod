/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <algorithm>

#include <QPainter>
#include <QVariantAnimation>
#include <QWidget>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/viewers/viewport/viewport_style.hpp"

namespace hesiod::viewport
{

// =====================================
// SnapGuide: where the dragged rail will land
// =====================================

class SnapGuide final : public QWidget
{
public:
  explicit SnapGuide(QWidget *parent) : QWidget(parent)
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

  void set_target(const QRectF &rect)
  {
    this->target = rect;
    this->update();
  }

  void appear()
  {
    this->setGeometry(this->parentWidget()->rect());
    this->show();
    this->raise();
    this->run_fade(1.0);
  }

  void vanish() { this->run_fade(0.0); }

protected:
  void paintEvent(QPaintEvent *) override
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
    p.setPen(QPen(edge, 1.5, Qt::DashLine));
    p.setBrush(fill);
    const qreal r = std::min(10.0,
                             std::min(this->target.width(), this->target.height()) / 2);
    p.drawRoundedRect(this->target.adjusted(0.75, 0.75, -0.75, -0.75), r, r);
  }

private:
  void run_fade(qreal to)
  {
    this->fade->stop();
    this->fade->setStartValue(this->opacity);
    this->fade->setEndValue(to);
    this->fade->setDuration(anim_ms(140));
    this->fade->start();
  }

  QRectF             target;
  qreal              opacity = 0.0;
  QVariantAnimation *fade = nullptr;
};

} // namespace hesiod::viewport
