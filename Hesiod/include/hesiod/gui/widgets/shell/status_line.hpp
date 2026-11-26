/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <QString>
#include <QTimer>
#include <QWidget>

#include "hesiod/gui/motion/motion.hpp"

namespace hesiod
{

// The message line of the status bar. A message fades in and rises a few
// pixels into place, gives way to the next one through a quick fade, and
// fades out when its time is up, instead of the text switching in place.
class StatusLine : public QWidget
{
public:
  explicit StatusLine(QWidget *parent = nullptr);

  // timeout_ms <= 0: stays until the next message
  void show_message(const QString &message, int timeout_ms);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  void appear(int timeout_ms); // fades `text` in and starts its time

  QString text;    // shown now
  QString pending; // shown once the current one has faded
  bool    has_pending = false;
  int     pending_timeout = 0;
  QTimer  expiry;
  Anim    fade{this}; // 0 gone .. 1 shown
};

} // namespace hesiod
