/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <QAbstractButton>
#include <QImage>
#include <QPixmap>

#include "hesiod/gui/motion/motion.hpp"
#include "hesiod/gui/widgets/dialogs/start_screen/arrival.hpp"

namespace hesiod
{

// An example terrain on the start screen, shown as its picture. Nothing is
// drawn around it: the picture has rounded corners, its name sits on a soft
// shade along the bottom, and under the pointer the picture draws a little
// closer and an arrow appears where a click will take you. The hero tile is
// the large one that leads the set.
class ExampleTile final : public QAbstractButton
{
  Q_OBJECT

public:
  ExampleTile(const QString &title,
              const QString &preview_path,
              bool           hero,
              QWidget       *parent = nullptr);

  Arrival arrival;

protected:
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  void draw_placeholder(QPainter &p, const QRectF &box) const;

  QString title;
  bool    hero = false;
  QImage  source;     // read no wider than it will ever be shown
  QPixmap scaled;     // the source at the size and scale it is shown at
  QSize   scaled_for; // the size `scaled` was made for
  Anim    hover_t{this};
  Anim    zoom_t{this};
  Anim    press_t{this};
};

} // namespace hesiod
