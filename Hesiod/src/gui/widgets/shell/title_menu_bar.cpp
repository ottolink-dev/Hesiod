/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include <QAction>
#include <QActionEvent>
#include <QMenu>
#include <QPainter>
#include <QStyle>

#include "meta_qt/ui/theme.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/shell/title_menu_bar.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"

namespace hesiod
{

TitleMenuBar::TitleMenuBar(QWidget *parent) : QMenuBar(parent), hot(this) {}

// Which entry is lit is Qt's business (pointer, keyboard, open menu); the fade
// only follows it. QMenuBar has no signal for its active entry going back to
// none, so it is read after each event the bar handles: the pointer and keys
// reach the bar as events, also while one of its menus is open. A menu closing
// clears the entry after its aboutToHide, hence the queued call for that case.
bool TitleMenuBar::event(QEvent *event)
{
  const bool handled = QMenuBar::event(event);

  if (event->type() == QEvent::ActionAdded)
  {
    QAction *action = static_cast<QActionEvent *>(event)->action();
    if (QMenu *menu = action ? action->menu() : nullptr)
      QObject::connect(menu,
                       &QMenu::aboutToHide,
                       this,
                       &TitleMenuBar::follow_active,
                       Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection));
  }

  if (event->type() != QEvent::Paint)
    this->follow_active();

  return handled;
}

void TitleMenuBar::follow_active()
{
  QAction *active = this->activeAction();
  this->hot.set(active ? static_cast<int>(this->actions().indexOf(active)) : -1);
}

void TitleMenuBar::paintEvent(QPaintEvent *)
{
  const auto            &colors = HSD_CTX.app_settings.colors;
  const QList<QAction *> entries = this->actions();
  QAction               *active = this->activeAction();

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::SmoothPixmapTransform);
  p.setFont(meta::qt::ui_font(13));

  const QColor rest = mix_colors(colors.text_primary, colors.bg_deep, 0.35);
  const int    icon = this->style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this);

  for (int k = 0; k < entries.size(); ++k)
  {
    QAction *action = entries[k];
    if (action->isSeparator() || !action->isVisible())
      continue;

    const QRect cell = this->actionGeometry(action);
    if (cell.isEmpty())
      continue;

    const qreal heat = std::clamp(this->hot.heat(k), 0.0, 1.0);
    const bool  open = action == active && action->menu() && action->menu()->isVisible();

    if (heat > 0.002)
    {
      QColor plate = mix_colors(colors.bg_deep, colors.text_primary, open ? 0.13 : 0.08);
      plate.setAlphaF(heat);
      p.setPen(Qt::NoPen);
      p.setBrush(plate);
      p.drawRoundedRect(QRectF(cell), 6, 6);
    }

    // an entry that is only an icon (the application menu)
    if (action->text().isEmpty() && !action->icon().isNull())
    {
      const QRect at(cell.x() + (cell.width() - icon) / 2,
                     cell.y() + (cell.height() - icon) / 2,
                     icon,
                     icon);
      action->icon().paint(&p, at);
      continue;
    }

    QColor ink = mix_colors(rest, colors.text_primary, heat);
    if (!action->isEnabled())
      ink = mix_colors(ink, colors.bg_deep, 0.45);
    p.setPen(ink);
    p.drawText(cell, Qt::AlignCenter | Qt::TextHideMnemonic, action->text());
  }
}

} // namespace hesiod
