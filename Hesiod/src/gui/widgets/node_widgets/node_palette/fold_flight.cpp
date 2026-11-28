/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <utility>

#include <QPainter>
#include <QPainterPath>
#include <QTimer>

#include "hesiod/gui/widgets/node_widgets/node_palette/fold_flight.hpp"
#include "hesiod/gui/widgets/node_widgets/node_palette/node_palette_style.hpp"

namespace hesiod
{

namespace np = node_palette;

FoldFlight::FoldFlight(QWidget              *host,
                       const QRect          &bounds,
                       const QPixmap        &backdrop,
                       const Shot           &from,
                       const Shot           &to,
                       const QPixmap        &rail,
                       const Look           &look,
                       std::function<void()> on_landed)
    : QWidget(host), backdrop(backdrop), from(from), to(to), rail(rail), look(look),
      on_landed(std::move(on_landed)), progress(this, 0.0)
{
  this->setAttribute(Qt::WA_OpaquePaintEvent);
  this->setFocusPolicy(Qt::NoFocus);
  this->setGeometry(bounds);
  this->raise();
  this->show();

  this->progress.on_done = [this]()
  {
    // the widgets come back in the same turn as this picture goes, so the
    // screen never shows a moment without either
    if (this->on_landed)
      this->on_landed();
    this->hide();
    this->deleteLater();
  };

  // Folding gathers and settles; unfolding arrives and decelerates. The move
  // starts a moment after the click, not on it: the click leaves work behind
  // (its tooltip closing, the widgets that were just hidden) and that work
  // costs a frame. Spent while the picture still stands, it is not seen.
  const bool folding = to.reveal < from.reveal;
  QTimer::singleShot(motion::library_fold_lead,
                     this,
                     [this, folding]()
                     {
                       this->progress.to(1.0,
                                         folding ? motion::library_fold
                                                 : motion::library_unfold,
                                         folding ? motion::change() : motion::enter());
                     });
}

void FoldFlight::paintEvent(QPaintEvent *)
{
  QPainter p(this);
  p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
  p.drawPixmap(this->rect(), this->backdrop);

  const double u = std::clamp(this->progress.value(), 0.0, 1.0);
  auto         mix = [u](double a, double b) { return a + (b - a) * u; };

  const QRectF card(mix(this->from.card.left(), this->to.card.left()),
                    mix(this->from.card.top(), this->to.card.top()),
                    mix(this->from.card.width(), this->to.card.width()),
                    mix(this->from.card.height(), this->to.card.height()));
  const double r = std::min(this->look.radius,
                            std::min(card.width(), card.height()) / 2.0);

  QPainterPath shape;
  shape.addRoundedRect(card, r, r);
  p.setPen(QPen(this->look.seam, 1));
  p.setBrush(this->look.card_bg);
  p.drawPath(shape);

  // the two states of the content, neither moving: the unfolded one is wiped
  // by the edge and fades ahead of it, the folded one comes up as it goes.
  // Their glyphs are in the same place, so the glyphs simply stay.
  const double reveal = mix(this->from.reveal, this->to.reveal);
  const Shot  &open = this->from.reveal > this->to.reveal ? this->from : this->to;
  const Shot  &shut = this->from.reveal > this->to.reveal ? this->to : this->from;

  p.save();
  p.setClipPath(shape);
  p.setOpacity(1.0 - np::smoothstep(np::folded_fade_from, np::folded_fade_to, reveal));
  p.drawPixmap(shut.origin, shut.content);
  p.setOpacity(np::smoothstep(np::unfolded_fade_from, np::unfolded_fade_to, reveal));
  p.drawPixmap(open.origin, open.content);
  p.restore();

  if (!this->rail.isNull())
    p.drawPixmap(QPointF(mix(this->from.rail_pos.x(), this->to.rail_pos.x()),
                         mix(this->from.rail_pos.y(), this->to.rail_pos.y())),
                 this->rail);
}

} // namespace hesiod
