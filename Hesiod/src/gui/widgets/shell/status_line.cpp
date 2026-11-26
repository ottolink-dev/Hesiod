/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cmath>
#include <utility>

#include <QFontMetrics>
#include <QPainter>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/shell/shell_style.hpp"
#include "hesiod/gui/widgets/shell/status_line.hpp"

namespace hesiod
{

StatusLine::StatusLine(QWidget *parent) : QWidget(parent)
{
  this->setAttribute(Qt::WA_TransparentForMouseEvents);
  this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

  // only the text is drawn: without this the application stylesheet fills the
  // widget with the pane color, a box on the status bar
  this->setAttribute(Qt::WA_NoSystemBackground);

  this->expiry.setSingleShot(true);
  QObject::connect(&this->expiry,
                   &QTimer::timeout,
                   this,
                   [this]()
                   { this->fade.to(0.0, motion::status_out, motion::change()); });

  // faded out: either the next message is waiting, or the line is now empty
  this->fade.on_done = [this]()
  {
    if (this->fade.value() > 0.0)
      return;

    this->text.clear();
    if (std::exchange(this->has_pending, false))
    {
      this->text = std::exchange(this->pending, QString());
      this->appear(this->pending_timeout);
    }
    this->update();
  };
}

void StatusLine::appear(int timeout_ms)
{
  if (this->text.isEmpty())
    return;

  this->fade.to(1.0, motion::status_in, motion::enter());
  if (timeout_ms > 0)
    this->expiry.start(timeout_ms);
}

void StatusLine::show_message(const QString &message, int timeout_ms)
{
  this->expiry.stop();

  // the same text again only renews its time
  if (message == this->text && !this->has_pending && !message.isEmpty())
  {
    this->appear(timeout_ms);
    return;
  }

  // nothing is showing: the message comes in directly
  if (this->text.isEmpty() || this->fade.value() <= 0.0)
  {
    this->has_pending = false;
    this->text = message;
    this->fade.set(0.0);
    this->appear(timeout_ms);
    return;
  }

  // Something is showing: it steps aside first. A message that arrives while
  // that is under way only replaces the one waiting, so a burst of messages
  // does not keep restarting the fade.
  this->pending = message;
  this->pending_timeout = timeout_ms;
  if (std::exchange(this->has_pending, true))
    return;

  this->fade.set(this->fade.value()); // it may be on its slow way out already
  this->fade.to(0.0, motion::status_aside, motion::leave());
}

QSize StatusLine::sizeHint() const
{
  return QSize(shell::status_width_hint,
               QFontMetrics(this->font()).height() + shell::status_height_padding);
}

QSize StatusLine::minimumSizeHint() const
{
  return QSize(shell::status_min_width,
               QFontMetrics(this->font()).height() + shell::status_height_padding);
}

void StatusLine::paintEvent(QPaintEvent *)
{
  const qreal shown = std::clamp(this->fade.value(), 0.0, 1.0);
  if (this->text.isEmpty() || shown <= 0.0)
    return;

  QPainter p(this);
  p.setRenderHint(QPainter::TextAntialiasing);

  QColor ink = HSD_CTX.app_settings.colors.text_secondary;
  ink.setAlphaF(ink.alphaF() * shown);
  p.setPen(ink);

  // It rises the last few pixels into place as it appears, a whole device
  // pixel at a time: text drawn between two pixel rows is soft.
  const qreal dpr = this->devicePixelRatioF();
  const qreal rise = std::round(shell::status_rise * (1.0 - shown) * dpr) / dpr;

  const QRectF  box = QRectF(this->rect()).adjusted(shell::status_inset, 0.0, 0.0, 0.0);
  const QString line = QFontMetrics(this->font())
                           .elidedText(this->text,
                                       Qt::ElideRight,
                                       std::max(0, this->width() - shell::status_inset));
  p.drawText(box.translated(0.0, rise), Qt::AlignLeft | Qt::AlignVCenter, line);
}

} // namespace hesiod
