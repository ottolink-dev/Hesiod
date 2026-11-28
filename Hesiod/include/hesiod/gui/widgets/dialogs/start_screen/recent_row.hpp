/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <QAbstractButton>

#include "hesiod/gui/motion/motion.hpp"
#include "hesiod/gui/widgets/dialogs/start_screen/arrival.hpp"

namespace hesiod
{

// A project opened lately, in the start screen's rail: its name, and under it
// when it was saved and where it lives.
class RecentRow final : public QAbstractButton
{
  Q_OBJECT

public:
  RecentRow(const QString &title,
            const QString &when,
            const QString &where,
            QWidget       *parent = nullptr);

  Arrival arrival;

protected:
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  QString title;
  QString when;
  QString where;
  Anim    hover_t{this};
};

} // namespace hesiod
