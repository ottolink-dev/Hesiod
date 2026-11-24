/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/gui/motion/motion.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include <QAnimationDriver>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QGuiApplication>
#include <QObject>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <QVector>
#include <QWidget>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <dwmapi.h>
#endif

namespace hesiod
{

// =====================================
// AnimClock
// =====================================

// One clock for every running Anim. It only runs while something is moving,
// so an idle window costs no wake-ups.
//
// What makes motion look smooth is not how often it is stepped but that every
// displayed frame shows a position sampled at an even spacing in time. A
// plain timer drifts against the display: some frames get two steps, some
// none, and a steady glide turns into a stutter. So on Windows each step
// first waits for the compositor to finish a frame (DwmFlush), which locks
// the steps to the display's own rhythm, whatever its refresh rate.
class AnimClock : public QObject
{
public:
  static AnimClock &get()
  {
    // owned by the application object, so its timer dies with the event loop
    // and not during static destruction after it
    static QPointer<AnimClock> instance;
    if (!instance)
      instance = new AnimClock(QCoreApplication::instance());
    return *instance;
  }

  qint64 now() const { return this->elapsed.nsecsElapsed(); }

  void add(Anim *anim)
  {
    if (!this->running.contains(anim))
      this->running.push_back(anim);
    this->wake();
  }

  void remove(Anim *anim)
  {
    this->running.removeAll(anim);
    if (this->running.isEmpty() && !this->on_frame)
      this->timer.stop();
  }

  // Something else that wants to be called once per displayed frame (Qt's
  // animation driver); an empty function ends it.
  void drive(std::function<void()> frame)
  {
    this->on_frame = std::move(frame);
    if (this->on_frame)
      this->wake();
    else if (this->running.isEmpty())
      this->timer.stop();
  }

  void wake()
  {
    if (!this->timer.isActive())
    {
      this->timer.setInterval(this->paced ? 1 : this->fallback_interval());
      this->timer.start();
      this->frame_ns = 1.0e9 / this->refresh_rate();
      this->last_tick = 0; // the gap since the last burst of motion is not a frame
    }
  }

private:
  explicit AnimClock(QObject *parent) : QObject(parent)
  {
    this->elapsed.start();
    this->timer.setTimerType(Qt::PreciseTimer);
    QObject::connect(&this->timer, &QTimer::timeout, this, [this]() { this->tick(); });
#ifdef Q_OS_WIN
    BOOL composited = FALSE;
    this->paced = SUCCEEDED(::DwmIsCompositionEnabled(&composited)) && composited;
#endif
  }

  double refresh_rate() const
  {
    qreal rate = 60.0;
    for (const QScreen *screen : QGuiApplication::screens())
      rate = std::max(rate, screen->refreshRate());
    return rate;
  }

  int fallback_interval() const
  {
    return std::clamp(static_cast<int>(1000.0 / this->refresh_rate()), 4, 16);
  }

  void tick()
  {
#ifdef Q_OS_WIN
    // Only wait if this step comes early. A window that is presented through
    // a swap chain (anything holding an OpenGL viewport) already blocks in
    // its own paint until the next refresh; waiting again there would halve
    // the frame rate.
    const bool early = this->last_tick == 0 ||
                       static_cast<double>(this->now() - this->last_tick) <
                           0.7 * this->frame_ns;
    if (this->paced && early)
    {
      // returns when the compositor has taken the frame painted after the
      // previous step: from here to the next one is exactly one refresh
      const qint64 before = this->now();
      if (FAILED(::DwmFlush()))
        this->quick_returns = 1000;
      else if (this->now() - before < 300000) // under 0.3 ms: it did not wait
        ++this->quick_returns;
      else
        this->quick_returns = 0;

      // no compositor to wait for (remote session, composition off): fall
      // back to a timer at the refresh rate rather than spin
      if (this->quick_returns > 40)
      {
        this->paced = false;
        this->timer.setInterval(this->fallback_interval());
      }
    }
#endif

    const qint64 now = this->now();
    if (this->last_tick != 0)
    {
      const double gap = static_cast<double>(now - this->last_tick) / 1.0e6;
      ++this->frames;
      this->sum += gap;
      this->sum_squares += gap * gap;
      this->worst = std::max(this->worst, gap);
    }
    this->last_tick = now;

    // a step may start or stop other animations, so walk a copy
    const QVector<Anim *> snapshot = this->running;
    for (Anim *anim : snapshot)
      if (this->running.contains(anim))
        anim->step(now);

    if (this->on_frame)
    {
      const std::function<void()> frame = this->on_frame; // it may end itself
      frame();
    }

    // Put this step on screen before the next one is taken. Stepping only
    // asks for a repaint, and the repaint waits in the event queue behind the
    // clock's own next timeout, which is already due because this step spent
    // a whole refresh waiting for the compositor. Left to the queue, two steps
    // ran back to back and only the second was ever painted: measured in the
    // main window, a frame every 33 ms instead of every 16.7, with every
    // other position skipped. Painting here makes it one step, one frame.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::UpdateRequest);
  }

  QTimer                timer;
  QElapsedTimer         elapsed;
  QVector<Anim *>       running;
  std::function<void()> on_frame;
  bool                  paced = false;
  int                   quick_returns = 0;
  double                frame_ns = 1.0e9 / 60.0;

  qint64 last_tick = 0;
  int    frames = 0;
  double sum = 0.0;
  double sum_squares = 0.0;
  double worst = 0.0;

public:
  motion::FramePacing pacing() const
  {
    motion::FramePacing out;
    out.frames = this->frames;
    out.display_locked = this->paced;
    if (this->frames > 0)
    {
      out.mean_ms = this->sum / this->frames;
      out.deviation_ms = std::sqrt(
          std::max(0.0, this->sum_squares / this->frames - out.mean_ms * out.mean_ms));
      out.worst_ms = this->worst;
    }
    return out;
  }
};

// =====================================
// motion
// =====================================

namespace motion
{

namespace
{

double g_time_scale = 1.0;
bool   g_enabled = true;

QEasingCurve bezier(double x1, double y1, double x2, double y2)
{
  QEasingCurve curve(QEasingCurve::BezierSpline);
  curve.addCubicBezierSegment(QPointF(x1, y1), QPointF(x2, y2), QPointF(1.0, 1.0));
  return curve;
}

// The step response of an under-damped spring: it starts from rest, runs up
// to the target, passes it by about five per cent and settles. Unlike a
// bezier with a control point above 1, its speed is continuous from the first
// frame to the last, which is what the eye reads as weight.
qreal spring_response(qreal x)
{
  constexpr double decay = 7.5;
  constexpr double frequency = 8.0;
  if (x >= 1.0)
    return 1.0;
  return 1.0 - std::exp(-decay * x) * (std::cos(frequency * x) +
                                       (decay / frequency) * std::sin(frequency * x));
}

} // namespace

const QEasingCurve &enter()
{
  static const QEasingCurve curve = bezier(0.22, 1.0, 0.36, 1.0);
  return curve;
}

const QEasingCurve &leave()
{
  static const QEasingCurve curve = bezier(0.40, 0.0, 0.90, 0.60);
  return curve;
}

const QEasingCurve &change()
{
  static const QEasingCurve curve = bezier(0.45, 0.0, 0.15, 1.0);
  return curve;
}

const QEasingCurve &spring()
{
  static const QEasingCurve curve = []()
  {
    QEasingCurve custom;
    custom.setCustomType(spring_response);
    return custom;
  }();
  return curve;
}

double time_scale() { return g_time_scale; }

void set_time_scale(double scale) { g_time_scale = std::clamp(scale, 0.1, 20.0); }

bool enabled() { return g_enabled; }

void set_enabled(bool enabled) { g_enabled = enabled; }

FramePacing frame_pacing() { return AnimClock::get().pacing(); }

namespace
{

// Hands Qt's animation timer over to AnimClock: Qt asks for frames while any
// of its animations runs, and is advanced once per displayed frame.
class FrameDriver final : public QAnimationDriver
{
public:
  using QAnimationDriver::QAnimationDriver;

protected:
  void start() override
  {
    QAnimationDriver::start();
    AnimClock::get().drive(
        [this]()
        {
          if (this->isRunning())
            this->advance();
        });
  }

  void stop() override
  {
    AnimClock::get().drive(nullptr);
    QAnimationDriver::stop();
  }
};

} // namespace

void drive_qt_animations()
{
  static bool installed = false;
  if (installed || !QCoreApplication::instance())
    return;
  installed = true;

  auto *driver = new FrameDriver(QCoreApplication::instance());
  driver->install();
}

bool system_prefers_motion()
{
#ifdef Q_OS_WIN
  BOOL animations = TRUE;
  if (::SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0))
    return animations != FALSE;
#endif
  return true;
}

} // namespace motion

// =====================================
// Anim
// =====================================

Anim::Anim(double initial) : current(initial), end(initial) {}

Anim::Anim(QWidget *repaint, double initial) : current(initial), end(initial)
{
  QPointer<QWidget> guard(repaint);
  this->on_tick = [guard]()
  {
    if (guard)
      guard->update();
  };
}

Anim::~Anim()
{
  *this->alive = false;
  if (this->active)
    AnimClock::get().remove(this);
}

void Anim::set(double value)
{
  if (this->active)
  {
    this->active = false;
    AnimClock::get().remove(this);
  }
  ++this->run;
  this->current = value;
  this->end = value;
  if (this->on_tick)
  {
    const std::function<void()> tick = this->on_tick;
    tick(); // last: the callback may destroy this
  }
}

void Anim::to(double target, int duration_ms, const QEasingCurve &easing)
{
  if (this->active && target == this->end)
    return;
  if (!this->active && target == this->current)
  {
    this->end = target;
    return;
  }

  if (duration_ms <= 0 || !motion::enabled())
  {
    const std::shared_ptr<bool> token = this->alive;
    this->set(target);
    if (!*token)
      return;
    const unsigned mine = this->run;
    if (this->on_done)
    {
      const std::function<void()> done = this->on_done;
      if (mine == this->run)
        done();
    }
    return;
  }

  ++this->run;
  this->start = this->current;
  this->end = target;
  this->curve = easing;
  this->start_ns = AnimClock::get().now();
  this->duration_ns = static_cast<qint64>(duration_ms * motion::time_scale() * 1.0e6);
  this->active = true;
  AnimClock::get().add(this);
}

void Anim::step(qint64 now_ns)
{
  const double progress = this->duration_ns > 0
                              ? static_cast<double>(now_ns - this->start_ns) /
                                    static_cast<double>(this->duration_ns)
                              : 1.0;

  if (progress >= 1.0)
  {
    const std::shared_ptr<bool> token = this->alive;
    const unsigned              mine = this->run;

    this->current = this->end;
    this->active = false;
    AnimClock::get().remove(this);

    if (this->on_tick)
    {
      const std::function<void()> tick = this->on_tick;
      tick();
    }
    // the tick may have destroyed this, or started the next run
    if (!*token || mine != this->run || this->active)
      return;
    if (this->on_done)
    {
      const std::function<void()> done = this->on_done;
      done();
    }
    return;
  }

  const double eased = this->curve.valueForProgress(std::max(progress, 0.0));
  this->current = this->start + (this->end - this->start) * eased;
  if (this->on_tick)
  {
    const std::function<void()> tick = this->on_tick;
    tick();
  }
}

// =====================================
// HotTrack
// =====================================

HotTrack::HotTrack(QWidget *repaint) : widget(repaint) {}

HotTrack::~HotTrack() = default;

void HotTrack::set(int index)
{
  if (index == this->current)
    return;

  // items that have finished cooling are forgotten
  std::erase_if(this->entries,
                [this](const Entry &entry)
                {
                  return entry.index != this->current && !entry.anim->running() &&
                         entry.anim->value() <= 0.0;
                });

  for (Entry &entry : this->entries)
    if (entry.index == this->current)
      entry.anim->to(0.0, motion::hover_out, motion::change());

  this->current = index;
  if (index < 0)
    return;

  for (Entry &entry : this->entries)
    if (entry.index == index)
    {
      entry.anim->to(1.0, motion::hover_in, motion::enter());
      return;
    }

  Entry entry;
  entry.index = index;
  entry.anim = std::make_unique<Anim>(this->widget, 0.0);
  entry.anim->to(1.0, motion::hover_in, motion::enter());
  this->entries.push_back(std::move(entry));
}

double HotTrack::heat(int index) const
{
  for (const Entry &entry : this->entries)
    if (entry.index == index)
      return entry.anim->value();
  return 0.0;
}

} // namespace hesiod
