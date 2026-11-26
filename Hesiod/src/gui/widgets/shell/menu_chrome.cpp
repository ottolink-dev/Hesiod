/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cmath>

#include <QHideEvent>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QShowEvent>
#include <QStyleOptionMenuItem>
#include <QWidgetAction>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/shell/menu_chrome.hpp"
#include "hesiod/gui/widgets/shell/shell_style.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"

namespace hesiod
{

namespace
{

constexpr char header_tag[] = "hsdHeader"; // marks the action of a header row

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

HsdMenu::HsdMenu(const QString &title, QWidget *parent) : QMenu(title, parent)
{
  // QMenu#hsdMenu in the stylesheet: no fill of its own, see paintEvent()
  this->setObjectName("hsdMenu");

  // Which row is active is Qt's business (pointer, keys, an open submenu); the
  // highlight only follows it. The signal covers a row becoming active, also
  // from the keyboard; it arrives for the rows of a submenu too, hence
  // activeAction() and not the action it carries. No row being active any
  // more has no signal: see leaveEvent() and mouseMoveEvent().
  this->connect(this, &QMenu::hovered, this, [this]() { this->follow_active(); });
}

HsdMenu *HsdMenu::add_sub_menu(const QString &title)
{
  auto *menu = new HsdMenu(title, this);
  this->addMenu(menu);
  return menu;
}

QAction *HsdMenu::add_header(const QString &title)
{
  QAction *action = this->addAction(title);
  action->setEnabled(false);
  action->setProperty(header_tag, true);
  return action;
}

void HsdMenu::follow_active()
{
  QAction    *action = this->activeAction();
  const QRect at = action ? this->actionGeometry(action) : QRect();

  // rows that cannot be chosen, or that hold a widget, take no highlight
  const bool takes = action && action->isVisible() && action->isEnabled() &&
                     !action->isSeparator() && !qobject_cast<QWidgetAction *>(action) &&
                     at.isValid() && this->contentsRect().intersects(at);
  if (!takes)
  {
    this->plate_alpha.to(0.0, motion::hover_out, motion::leave());
    return;
  }

  // The highlight glides from a row to its neighbor, which is what reads as
  // one highlight moving. Sent further than that (across a separator, or the
  // pointer coming back in somewhere else) it simply is there: a highlight
  // still on its way while the pointer has stopped reads as lag.
  const bool far = std::abs(at.y() - this->plate_y.target()) > 1.6 * at.height() ||
                   at.x() != this->row.x();

  if (!this->lit || this->plate_alpha.value() < 0.03 || far)
  {
    this->plate_y.set(at.y());
    this->plate_height.set(at.height());
  }
  else
  {
    // decelerating from the first frame, so it leaves with the pointer
    this->plate_y.to(at.y(), motion::menu_glide, motion::enter());
    this->plate_height.to(at.height(), motion::menu_glide, motion::enter());
  }

  this->row = at;
  this->lit = true;
  this->plate_alpha.to(1.0, motion::hover_in, motion::enter());
}

void HsdMenu::showEvent(QShowEvent *event)
{
  this->lit = false;
  this->plate_alpha.set(0.0);
  QMenu::showEvent(event);
  this->follow_active(); // opened from the keyboard, the first row is active
}

void HsdMenu::hideEvent(QHideEvent *event)
{
  this->lit = false;
  this->plate_alpha.set(0.0);
  QMenu::hideEvent(event);
}

void HsdMenu::leaveEvent(QEvent *event)
{
  QMenu::leaveEvent(event);
  this->follow_active();
}

void HsdMenu::mouseMoveEvent(QMouseEvent *event)
{
  QMenu::mouseMoveEvent(event);
  this->follow_active();
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

  // a header is a disabled row (the stylesheet gives it that ink), set apart
  // from the rows that are merely unavailable by its weight
  if (action && action->property(header_tag).toBool())
    option->font.setWeight(QFont::DemiBold);
}

void HsdMenu::paintEvent(QPaintEvent *event)
{
  const auto &colors = HSD_CTX.app_settings.colors;

  // the fill and the highlight go under what QMenu draws (its rows and the
  // frame), which is why the stylesheet gives those no background of their own
  {
    QPainter p(this);
    p.fillRect(event->rect(), popup_color());

    const qreal alpha = std::clamp(this->plate_alpha.value(), 0.0, 1.0);
    if (this->lit && alpha > 0.0)
    {
      QColor fill = menu_hover_color();
      fill.setAlphaF(fill.alphaF() * alpha);

      p.setRenderHint(QPainter::Antialiasing);
      p.setClipRect(this->contentsRect());
      p.setPen(Qt::NoPen);
      p.setBrush(fill);
      p.drawRoundedRect(QRectF(this->row.x(),
                               this->plate_y.value(),
                               this->row.width(),
                               this->plate_height.value()),
                        shell::menu_plate_radius,
                        shell::menu_plate_radius);
    }
  }

  QMenu::paintEvent(event);

  QFont font = this->font();
  if (font.pointSizeF() > 0)
    font.setPointSizeF(font.pointSizeF() * shell::menu_shortcut_scale);
  else
    font.setPixelSize(std::max(8, qRound(font.pixelSize() * shell::menu_shortcut_scale)));

  QPainter p(this);
  p.setFont(font);

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

    QColor ink = mix_colors(colors.bg_primary,
                            colors.text_primary,
                            shell::menu_shortcut_ink);
    if (!action->isEnabled())
      ink = colors.text_disabled;
    else if (action == this->activeAction())
      ink = mix_colors(colors.bg_primary,
                       colors.text_primary,
                       shell::menu_shortcut_ink_active);

    p.setPen(ink);
    p.drawText(rect.adjusted(0, 0, -shell::menu_shortcut_inset, 0),
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

} // namespace hesiod
