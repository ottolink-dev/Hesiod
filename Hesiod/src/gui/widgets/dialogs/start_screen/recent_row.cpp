/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include <QEnterEvent>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QPainter>

#include "hesiod/gui/widgets/controls/control_tones.hpp"
#include "hesiod/gui/widgets/dialogs/start_screen/recent_row.hpp"
#include "hesiod/gui/widgets/dialogs/start_screen/start_screen_style.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"
#include "meta_qt/ui/theme.hpp"

namespace hesiod
{

namespace ss = start_screen;

RecentRow::RecentRow(const QString &title,
                     const QString &when,
                     const QString &where,
                     QWidget       *parent)
    : QAbstractButton(parent), arrival(this), title(title), when(when), where(where)
{
  this->setCursor(Qt::PointingHandCursor);
  this->setFocusPolicy(Qt::StrongFocus);
  this->setFixedHeight(ss::recent_height);
  this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  this->setAttribute(Qt::WA_Hover);
  this->setAccessibleName(title);
  this->setAccessibleDescription(where);
}

void RecentRow::enterEvent(QEnterEvent *event)
{
  this->hover_t.to(1.0, motion::hover_in, motion::enter());
  QAbstractButton::enterEvent(event);
}

void RecentRow::leaveEvent(QEvent *event)
{
  this->hover_t.to(0.0, motion::hover_out, motion::change());
  QAbstractButton::leaveEvent(event);
}

void RecentRow::keyPressEvent(QKeyEvent *event)
{
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
  {
    this->click();
    return;
  }
  QAbstractButton::keyPressEvent(event);
}

void RecentRow::paintEvent(QPaintEvent *)
{
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  if (!this->arrival.apply(p, ss::row_rise))
    return;

  const ControlTones t = control_tones();
  const qreal        hover = std::clamp(
      std::max(this->hover_t.value(), this->hasFocus() ? ss::focus_heat : 0.0),
      0.0,
      1.0);
  const qreal  held = this->isDown() ? 1.0 : 0.0;
  const QRectF box = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);

  if (hover > 0.002)
  {
    p.setPen(Qt::NoPen);
    p.setBrush(
        with_alpha(mix_colors(t.content, t.ink, ss::plate_lift - ss::plate_press * held),
                   hover));
    p.drawRoundedRect(box, ss::row_radius, ss::row_radius);
  }

  const qreal left = box.left() + ss::row_inset;
  const qreal right = box.right() - ss::row_inset - 16.0 * hover; // room for the chevron

  const QFont title_font = meta::qt::ui_font(13);
  p.setFont(title_font);
  p.setPen(mix_colors(t.ink_dim, t.ink, 0.45 + 0.55 * hover));
  p.drawText(
      QRectF(left, box.top() + 6.0, right - left, 18.0),
      Qt::AlignLeft | Qt::AlignVCenter,
      QFontMetricsF(title_font).elidedText(this->title, Qt::ElideRight, right - left));

  // when it was last saved, then where it lives; the folder gives way first
  const QFont detail_font = meta::qt::ui_font(11);
  p.setFont(detail_font);
  p.setPen(t.ink_faint);
  const QFontMetricsF dm(detail_font);
  const QString       lead = this->when + QStringLiteral("  \u00B7  ");
  const qreal         lead_w = dm.horizontalAdvance(lead);
  const QString       place = dm.elidedText(this->where,
                                      Qt::ElideMiddle,
                                      std::max<qreal>(20.0, right - left - lead_w));
  p.drawText(QRectF(left, box.top() + 24.0, right - left, 16.0),
             Qt::AlignLeft | Qt::AlignVCenter,
             lead + place);

  if (hover > 0.01)
  {
    p.setPen(QPen(with_alpha(t.ink_dim, hover),
                  1.4,
                  Qt::SolidLine,
                  Qt::RoundCap,
                  Qt::RoundJoin));
    const QPointF c(box.right() - 15.0 - 3.0 * (1.0 - hover), box.center().y());
    p.drawLine(c + QPointF(-2.0, -4.0), c + QPointF(2.0, 0.0));
    p.drawLine(c + QPointF(2.0, 0.0), c + QPointF(-2.0, 4.0));
  }
}

} // namespace hesiod
