/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
// Motion: the durations and curves every transition uses, and a small value
// animator driven by one shared clock.
//
// Personality: calm with a little life. Things arrive and decelerate, leave a
// little faster. Toggles, segment indicators and toasts move on a damped
// spring; nothing else overshoots: a menu or a slider that bounces makes the
// user wait to see where it ends up.
//
// Every Anim honours interface.enable_ui_animations through
// motion::set_enabled(): with motion off, to() lands on its end state at once
// and reports completion, so callers need no second code path.
#pragma once
#include <functional>
#include <memory>
#include <vector>

#include <QEasingCurve>

class QWidget;

namespace hesiod
{

namespace motion
{

// milliseconds, before the global time scale
constexpr int hover_in = 130;
constexpr int hover_out = 220;
constexpr int press_in = 70;
constexpr int press_out = 180;
constexpr int focus_in = 150;
constexpr int focus_out = 130;
constexpr int popup_in = 190;
constexpr int popup_out = 130;
constexpr int tip_dwell = 500;
constexpr int tip_dwell_next = 90; // moving from one tool to its neighbour
constexpr int tip_in = 150;
constexpr int tip_out = 110;
constexpr int modal_in = 260;
constexpr int modal_out = 170;
constexpr int toast_in = 480;
constexpr int toast_out = 180;
constexpr int toast_dwell = 4200;
constexpr int window_in = 240;
constexpr int window_out = 170;
constexpr int segment = 420;
constexpr int toggle = 380;
constexpr int section_in = 260;
constexpr int section_out = 200;
constexpr int value = 300; // a control gliding to a value set from outside
constexpr int theme = 320; // colourway crossfade

// the shell's menus and status bar
constexpr int menu_glide = 85;   // a menu's highlight, from a row to its neighbor
constexpr int status_in = 200;   // a status bar message arriving
constexpr int status_aside = 90; // a status bar message stepping aside for the next
constexpr int status_out = 420;  // a status bar message whose time is up

const QEasingCurve &enter();  // (.22, 1, .36, 1)   decelerate into place
const QEasingCurve &leave();  // (.4, 0, .9, .6)     gather speed and go
const QEasingCurve &change(); // (.45, 0, .15, 1)   between two resting states
const QEasingCurve &spring(); // damped spring: from rest, ~5 % past, settle

// 1 = normal; larger values slow every transition down (for inspecting them)
double time_scale();
void   set_time_scale(double scale);

// false: every transition lands on its end state at once
bool enabled();
void set_enabled(bool enabled);

// honours the operating system's "show animations" preference
bool system_prefers_motion();

// How evenly the animation steps have been spaced since start: the number
// that decides whether motion looks smooth. For diagnostics.
struct FramePacing
{
  int    frames = 0;
  double mean_ms = 0.0;
  double deviation_ms = 0.0;
  double worst_ms = 0.0;
  bool   display_locked = false;
};
FramePacing frame_pacing();

// Qt's own animations -- QVariantAnimation, QPropertyAnimation and everything
// built on them, in this application and in the libraries it uses -- are by
// default stepped by a plain 16 ms timer, which drifts against the display
// and stutters exactly as described above. After this call they are stepped
// by the same display-locked clock as every Anim, so a section opening or a
// switch thrown in a library widget is as even as the motion drawn here.
// Call once, after the application object exists.
void drive_qt_animations();

} // namespace motion

// One animated number. Retargeting starts from the value currently shown, so
// an interrupted transition never jumps.
class Anim
{
public:
  explicit Anim(double initial = 0.0);
  // convenience: repaint this widget on every step
  explicit Anim(QWidget *repaint, double initial = 0.0);
  ~Anim();

  Anim(const Anim &) = delete;
  Anim &operator=(const Anim &) = delete;

  double value() const { return this->current; }
  double target() const { return this->end; }
  bool   running() const { return this->active; }

  void set(double value); // no transition
  void to(double target, int duration_ms, const QEasingCurve &curve);

  // Either may retarget the Anim or destroy its owner: both are called through
  // a copy, and nothing is read from the Anim afterwards without checking.
  std::function<void()> on_tick;
  std::function<void()> on_done;

private:
  friend class AnimClock;
  void step(qint64 now_ns);

  double       current = 0.0;
  double       start = 0.0;
  double       end = 0.0;
  qint64       start_ns = 0;
  qint64       duration_ns = 0;
  QEasingCurve curve;
  bool         active = false;
  unsigned     run = 0; // bumped by every set()/to(): a stale completion is dropped
  std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};

// Which item of a widget is under the pointer, when the widget paints many
// items itself (a toolbar's tools, a list's rows). The item the pointer just
// left fades out while the one it entered fades in, so moving along a row of
// tools leaves a short trail instead of a highlight that jumps.
class HotTrack
{
public:
  explicit HotTrack(QWidget *repaint);
  ~HotTrack();

  HotTrack(const HotTrack &) = delete;
  HotTrack &operator=(const HotTrack &) = delete;

  void   set(int index); // -1: nothing
  int    index() const { return this->current; }
  double heat(int index) const; // 0 cold .. 1 under the pointer

private:
  struct Entry
  {
    int                   index = -1;
    std::unique_ptr<Anim> anim;
  };

  QWidget           *widget = nullptr;
  std::vector<Entry> entries;
  int                current = -1;
};

} // namespace hesiod
