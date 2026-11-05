/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once

#include <QEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QScrollArea>
#include <QWidget>
#include <algorithm>

namespace hesiod
{

/**
 * @brief QScrollArea with a sizeHint that follows its content's natural height,
 * allowing parent dialogs (such as MessageDialog) to expand to fit all sections
 * when screen space permits, while still scrolling when constrained.
 */
class NaturalScrollArea : public QScrollArea
{
public:
  using QScrollArea::QScrollArea;

  QSize sizeHint() const override
  {
    if (const QWidget *w = this->widget())
      return QSize(w->sizeHint().width(), w->sizeHint().height());
    return QScrollArea::sizeHint();
  }

  QSize minimumSizeHint() const override { return QSize(0, 120); }

protected:
  bool eventFilter(QObject *watched, QEvent *event) override
  {
    if (watched == this->widget() &&
        (event->type() == QEvent::LayoutRequest || event->type() == QEvent::Resize))
      this->updateGeometry();
    return QScrollArea::eventFilter(watched, event);
  }
};

/**
 * @brief Clamps a dialog scroll area's maximum height so the dialog remains comfortably
 * bounded within the available screen height.
 */
inline void clamp_dialog_scroll_height(QScrollArea   *scroll_area,
                                       const QWidget *dialog,
                                       double         screen_height_ratio = 0.85,
                                       int            chrome_overhead = 180)
{
  if (!scroll_area || !dialog)
    return;

  const QWidget *anchor = dialog->parentWidget() ? dialog->parentWidget()->window()
                                                 : nullptr;
  const QScreen *screen = anchor && anchor->screen() ? anchor->screen()
                                                     : dialog->screen();
  if (!screen)
    screen = QGuiApplication::primaryScreen();

  const int available_height = screen ? screen->availableGeometry().height() : 800;
  const int max_dialog_height = std::max(300,
                                         int(available_height * screen_height_ratio));
  const int max_scroll_height = std::max(180, max_dialog_height - chrome_overhead);

  scroll_area->setMaximumHeight(max_scroll_height);
}

} // namespace hesiod
