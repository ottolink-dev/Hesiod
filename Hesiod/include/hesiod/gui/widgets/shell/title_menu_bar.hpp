/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <QMenuBar>

#include "hesiod/gui/motion/motion.hpp"

namespace hesiod
{

// The menu row of the title bar. Qt draws a menu bar's entries in two states,
// lit or not; this one paints them itself so the plate under the pointer fades
// in and out like every other control in the bar, and stays lit under an open
// menu. Sizes and spacing still come from the stylesheet (window_title_bar.css):
// only the drawing is replaced.
class TitleMenuBar final : public QMenuBar
{
public:
  explicit TitleMenuBar(QWidget *parent = nullptr);

protected:
  bool event(QEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  void follow_active(); // points the fade at Qt's active entry

  HotTrack hot; // which entry is lit, with the fade of the one just left
};

} // namespace hesiod
