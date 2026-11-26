/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QApplication>
#include <QDialog>
#include <QEvent>
#include <QMenu>

#include "hesiod/gui/motion/motion.hpp"
#include "hesiod/gui/widgets/shell/menu_animator.hpp"
#include "hesiod/gui/widgets/shell/popup_entrance.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"

namespace hesiod
{

// Rounded, antialiased corners and an edge in the given color, drawn by the
// window manager (Windows 11; a no-op elsewhere). Defined at the end of the
// file, after the Win32 headers, whose macros would clash with the code above.
void round_popup_corners(QWidget *popup, const QColor &edge, bool small_radius);

namespace
{

enum class Kind
{
  None,
  Popup,  // a menu, or the list of a combo box
  Tip,    // a tooltip
  Dialog, // a frameless dialog, which paints its own card and shadow
};

Kind kind_of(const QWidget *window)
{
  if (qobject_cast<const QMenu *>(window) ||
      window->inherits("QComboBoxPrivateContainer"))
    return Kind::Popup;

  if (window->inherits("QTipLabel"))
    return Kind::Tip;

  // Only a dialog over the window it belongs to. One shown before there is a
  // window (the start screen, a prompt at startup) is followed by loading,
  // which leaves no time to animate anything.
  const QWidget *owner = window->parentWidget();
  if (qobject_cast<const QDialog *>(window) && owner && owner->isVisible() &&
      window->windowFlags().testFlag(Qt::FramelessWindowHint))
    return Kind::Dialog;

  return Kind::None;
}

} // namespace

void MenuAnimator::install(QApplication &app)
{
  app.installEventFilter(new MenuAnimator(&app));
}

bool MenuAnimator::eventFilter(QObject *watched, QEvent *event)
{
  const QEvent::Type type = event->type();
  if ((type != QEvent::Show && type != QEvent::Hide) || !watched->isWidgetType())
    return false;

  auto *window = static_cast<QWidget *>(watched);
  if (!window->isWindow())
    return false;

  // hidden before it had landed: it must not come back half transparent
  if (type == QEvent::Hide)
  {
    PopupEntrance::cancel(window);
    return false;
  }

  const Kind kind = kind_of(window);
  if (kind == Kind::None)
    return false;

  if (kind != Kind::Dialog)
    round_popup_corners(window, seam_color(), kind == Kind::Tip);

  if (!motion::enabled())
    return false;

  // each a child of the window, removing itself when the window has landed
  switch (kind)
  {
  case Kind::Popup:
    new PopupEntrance(window, PopupEntrance::Travel::Unfold, motion::popup_in);
    break;
  case Kind::Tip:
    new PopupEntrance(window, PopupEntrance::Travel::None, motion::tip_in);
    break;
  case Kind::Dialog:
    new PopupEntrance(window, PopupEntrance::Travel::Rise, motion::modal_in);
    break;
  case Kind::None:
    break;
  }

  return false;
}

} // namespace hesiod

#ifdef Q_OS_WIN
#include <windows.h>

#include <dwmapi.h>

void hesiod::round_popup_corners(QWidget *popup, const QColor &edge, bool small_radius)
{
  if (!popup)
    return;

  const HWND hwnd = reinterpret_cast<HWND>(popup->winId());

  // DWMWCP_ROUND (2) is the 8 px radius the application's own controls use,
  // DWMWCP_ROUNDSMALL (3) is 4 px, right for something as small as a tooltip
  const DWORD corners = small_radius ? 3 : 2;
  ::DwmSetWindowAttribute(hwnd,
                          33 /* DWMWA_WINDOW_CORNER_PREFERENCE */,
                          &corners,
                          sizeof(corners));

  // the border the window manager draws along the rounded edge: the color of
  // the square one the stylesheet draws inside it
  const COLORREF color = RGB(edge.red(), edge.green(), edge.blue());
  ::DwmSetWindowAttribute(hwnd, 34 /* DWMWA_BORDER_COLOR */, &color, sizeof(color));
}
#else
void hesiod::round_popup_corners(QWidget *, const QColor &, bool) {}
#endif
