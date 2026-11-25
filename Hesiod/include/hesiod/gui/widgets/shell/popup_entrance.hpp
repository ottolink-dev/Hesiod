/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QWidget>

#include "hesiod/gui/motion/motion.hpp"

namespace hesiod
{

// The entrance of one transient window (a menu, a combo list, a tooltip, a
// frameless dialog): a short fade with a few pixels of travel into its place,
// stepped by the animation clock. Created as a child of the window when it is
// shown, see MenuAnimator; it removes itself once the window has landed.
//
// The window is moved by whole pixels and never scaled, so what it shows
// stays sharp on the way.
class PopupEntrance : public QObject
{
public:
  enum class Travel
  {
    None,   // fades in where it stands (a tooltip)
    Rise,   // comes up into place from just below (a dialog)
    Unfold, // comes out of what opened it: its parent menu, or the pointer
  };

  PopupEntrance(QWidget *window, Travel travel, int duration_ms);

  // Ends the entrance of this window, if one is running, and puts the window
  // in its place: hidden before it had landed, it must not come back half
  // transparent or a few pixels off.
  static void cancel(QWidget *window);

private:
  void start();
  void apply(double t); // t: 0 just shown .. 1 landed

  QPointer<QWidget> window;
  Travel            travel;
  int               duration_ms;
  QPoint            from;    // where it comes from, relative to its place
  QPoint            applied; // how far off its place it has been put
  Anim              progress;
};

} // namespace hesiod
