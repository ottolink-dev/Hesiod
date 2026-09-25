/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include <QApplication>
#include <QMenuBar>
#include <QPaintEvent>
#include <QPainter>
#include <QPointer>
#include <QStyleOptionMenuItem>
#include <QVariantAnimation>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/menu_chrome.hpp"

namespace hesiod
{

// Rounded, antialiased corners and a border in the menu's own colour, drawn by
// the window manager (Windows 11; a no-op elsewhere). Defined at the end of the
// file, after the Win32 headers, whose `interface` macro would clash with the
// settings above.
void round_popup_corners(QWidget *popup, const QColor &border);

namespace
{

QColor mix(const QColor &from, const QColor &to, qreal amount)
{
  return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * amount,
                          from.greenF() + (to.greenF() - from.greenF()) * amount,
                          from.blueF() + (to.blueF() - from.blueF()) * amount);
}

// the shortcut an action shows: an explicit "\t..." suffix in its text wins,
// as it does in Qt's own menu layout
QString shortcut_text(const QAction *action)
{
  const QString   text = action->text();
  const qsizetype tab = text.indexOf('\t');
  if (tab >= 0)
    return text.mid(tab + 1);

  return action->shortcut().toString(QKeySequence::NativeText);
}

} // namespace

// =====================================
// HsdMenu
// =====================================

HsdMenu::HsdMenu(const QString &title, QWidget *parent) : QMenu(title, parent) {}

HsdMenu *HsdMenu::add_sub_menu(const QString &title)
{
  auto *menu = new HsdMenu(title, this);
  this->addMenu(menu);
  return menu;
}

void HsdMenu::initStyleOption(QStyleOptionMenuItem *option, const QAction *action) const
{
  QMenu::initStyleOption(option, action);

  // Qt appends the shortcut after a tab and draws it with the item text; drop
  // it here and paint it ourselves in paintEvent. The column width Qt reserves
  // for it is computed from the action, not from this text, so it is kept.
  const qsizetype tab = option->text.indexOf('\t');
  if (tab >= 0)
    option->text.truncate(tab);
}

void HsdMenu::paintEvent(QPaintEvent *event)
{
  QMenu::paintEvent(event);

  const auto &colors = HSD_CTX.app_settings.colors;

  QFont font = this->font();
  if (font.pointSizeF() > 0)
    font.setPointSizeF(font.pointSizeF() * 0.86);
  else
    font.setPixelSize(std::max(8, qRound(font.pixelSize() * 0.86)));

  QPainter p(this);
  p.setFont(font);

  // right inset matching the item's right padding in the stylesheet
  const int right_inset = 18;

  for (QAction *action : this->actions())
  {
    if (!action->isVisible() || action->isSeparator() || action->menu())
      continue;

    const QString shortcut = shortcut_text(action);
    if (shortcut.isEmpty())
      continue;

    const QRect rect = this->actionGeometry(action);
    if (!rect.isValid() || !event->rect().intersects(rect))
      continue;

    QColor ink = mix(colors.bg_primary, colors.text_primary, 0.55);
    if (!action->isEnabled())
      ink = colors.text_disabled;
    else if (action == this->activeAction())
      ink = mix(colors.bg_primary, colors.text_primary, 0.80);

    p.setPen(ink);
    p.drawText(rect.adjusted(0, 0, -right_inset, 0),
               Qt::AlignRight | Qt::AlignVCenter,
               shortcut);
  }
}

HsdMenu *add_menu(QMenuBar *menu_bar, const QString &title)
{
  auto *menu = new HsdMenu(title, menu_bar);
  menu_bar->addMenu(menu);
  return menu;
}

// =====================================
// MenuAnimator
// =====================================

void MenuAnimator::install(QApplication &app)
{
  app.installEventFilter(new MenuAnimator(&app));
}

bool MenuAnimator::eventFilter(QObject *watched, QEvent *event)
{
  if (event->type() != QEvent::Show)
    return false;

  auto *menu = qobject_cast<QMenu *>(watched);
  if (!menu || !menu->isWindow())
    return false;

  {
    const auto &colors = HSD_CTX.app_settings.colors;
    round_popup_corners(menu, mix(colors.bg_primary, colors.border, 0.38));
  }

  if (!HSD_CTX.app_settings.interface.enable_ui_animations)
    return false;

  // Show is delivered before the native window appears, so starting from zero
  // opacity here means the menu never flashes in at full strength first
  const QPoint end = menu->pos();
  menu->setWindowOpacity(0.0);
  menu->move(end.x(), end.y() - drop_px);

  // a menu reopened before its last animation ended restarts cleanly
  if (auto *previous = menu->findChild<QVariantAnimation *>("_hsd_menu_anim",
                                                            Qt::FindDirectChildrenOnly))
  {
    previous->stop();
    previous->deleteLater();
  }

  auto *animation = new QVariantAnimation(menu);
  animation->setObjectName("_hsd_menu_anim");
  animation->setStartValue(0.0);
  animation->setEndValue(1.0);
  animation->setDuration(duration_ms);
  animation->setEasingCurve(QEasingCurve::OutCubic);

  QPointer<QMenu> guard(menu);
  QObject::connect(animation,
                   &QVariantAnimation::valueChanged,
                   menu,
                   [guard, end](const QVariant &value)
                   {
                     if (!guard || !guard->isVisible())
                       return;
                     const qreal t = value.toReal();
                     guard->setWindowOpacity(t);
                     guard->move(end.x(), end.y() - qRound(drop_px * (1.0 - t)));
                   });

  QObject::connect(animation,
                   &QVariantAnimation::finished,
                   menu,
                   [guard, end, animation]()
                   {
                     if (guard)
                     {
                       guard->setWindowOpacity(1.0);
                       if (guard->isVisible())
                         guard->move(end);
                     }
                     animation->deleteLater();
                   });

  animation->start();
  return false;
}

} // namespace hesiod

#ifdef Q_OS_WIN
#include <windows.h>

#include <dwmapi.h>

void hesiod::round_popup_corners(QWidget *popup, const QColor &border)
{
  if (!popup)
    return;

  const HWND hwnd = reinterpret_cast<HWND>(popup->winId());

  const DWORD round_small = 3; // DWMWCP_ROUNDSMALL: the system's popup radius
  ::DwmSetWindowAttribute(hwnd,
                          33 /* DWMWA_WINDOW_CORNER_PREFERENCE */,
                          &round_small,
                          sizeof(round_small));

  // the border DWM draws along the rounded edge: the stylesheet's
  // COLOR_PANEL_BORDER, so it matches the square one Qt paints inside
  const COLORREF edge = RGB(border.red(), border.green(), border.blue());
  ::DwmSetWindowAttribute(hwnd, 34 /* DWMWA_BORDER_COLOR */, &edge, sizeof(edge));
}
#else
void hesiod::round_popup_corners(QWidget *, const QColor &) {}
#endif
