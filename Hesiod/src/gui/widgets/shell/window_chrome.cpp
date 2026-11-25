/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cmath>

#include <QApplication>
#include <QEnterEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include "meta_qt/ui/theme.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/theme/style_sheet.hpp"
#include "hesiod/gui/widgets/shell/title_chip.hpp"
#include "hesiod/gui/widgets/shell/title_menu_bar.hpp"
#include "hesiod/gui/widgets/shell/window_chrome.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"

namespace hesiod
{

// =====================================
// WindowButton
// =====================================

WindowButton::WindowButton(Kind kind, QWidget *parent)
    : QAbstractButton(parent), kind(kind)
{
  this->setFocusPolicy(Qt::NoFocus);
  this->setCursor(Qt::ArrowCursor);
  this->setAttribute(Qt::WA_Hover);

  switch (kind)
  {
  case Kind::Minimize:
    this->setToolTip("Minimize");
    this->setAccessibleName("Minimize");
    break;
  case Kind::Maximize:
    this->setToolTip("Maximize");
    this->setAccessibleName("Maximize");
    break;
  case Kind::Close:
    this->setToolTip("Close");
    this->setAccessibleName("Close");
    break;
  }
}

void WindowButton::enterEvent(QEnterEvent *event)
{
  this->hovered = true;
  this->hover_t.to(1.0, motion::hover_in, motion::enter());
  QAbstractButton::enterEvent(event);
}

void WindowButton::leaveEvent(QEvent *event)
{
  this->hovered = false;
  this->hover_t.to(0.0, motion::hover_out, motion::leave());
  this->press_t.to(0.0, motion::press_out, motion::enter());
  QAbstractButton::leaveEvent(event);
}

void WindowButton::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton)
    this->press_t.to(1.0, motion::press_in, motion::enter());
  QAbstractButton::mousePressEvent(event);
}

void WindowButton::mouseReleaseEvent(QMouseEvent *event)
{
  this->press_t.to(0.0, motion::press_out, motion::enter());
  QAbstractButton::mouseReleaseEvent(event);
}

void WindowButton::set_maximized(bool new_state)
{
  if (this->maximized == new_state)
    return;

  this->maximized = new_state;
  if (this->kind == Kind::Maximize)
    this->setToolTip(new_state ? "Restore" : "Maximize");
  this->update();
}

QSize WindowButton::sizeHint() const { return QSize(36, 28); }

void WindowButton::paintEvent(QPaintEvent *)
{
  const auto &colors = HSD_CTX.app_settings.colors;

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  const QRectF box = QRectF(this->rect()).adjusted(2, 2, -2, -2);
  const QColor close_red("#c8463d");

  const qreal heat = std::clamp(std::max(this->hover_t.value(), this->press_t.value()),
                                0.0,
                                1.0);
  const qreal held = std::clamp(this->press_t.value(), 0.0, 1.0);

  if (heat > 0.002)
  {
    QColor plate;
    if (this->kind == Kind::Close)
      plate = mix_colors(close_red, close_red.darker(115), held);
    else
      plate = mix_colors(colors.bg_deep, colors.text_primary, 0.07 + 0.03 * held);
    plate.setAlphaF(heat);

    p.setPen(Qt::NoPen);
    p.setBrush(plate);
    p.drawRoundedRect(box, 8, 8);
  }

  // glyph colour: all three are neutral at rest. Nothing in the title bar is
  // coloured until it is aimed at; close turns light on its red plate.
  const QColor rest = mix_colors(colors.text_primary, colors.bg_deep, 0.30);
  const QColor lit = this->kind == Kind::Close ? QColor("#f4f4f5") : colors.text_primary;
  const QColor ink = mix_colors(rest, lit, heat);

  // all glyphs share one 10x10 box snapped to the pixel grid, so the three
  // buttons read as a set
  const qreal   g = 10.0;
  const QPointF c(std::round(this->width() / 2.0), std::round(this->height() / 2.0));
  const QRectF  glyph(c.x() - g / 2, c.y() - g / 2, g, g);

  // (the glyph stays on the pixel grid while held: the plate darkening is
  // the press feedback, a shrinking 10 px glyph only goes soft)

  QPen pen(ink, 1.2);
  pen.setCapStyle(Qt::RoundCap);
  pen.setJoinStyle(Qt::MiterJoin);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);

  switch (this->kind)
  {
  case Kind::Minimize:
  {
    QPen bar(ink, 1.6);
    bar.setCapStyle(Qt::RoundCap);
    p.setPen(bar);
    p.drawLine(QPointF(glyph.left(), c.y() + 0.5), QPointF(glyph.right(), c.y() + 0.5));
    break;
  }
  case Kind::Maximize:
    if (this->maximized)
    {
      // back square (only the visible L), then the front square
      const QRectF front = glyph.adjusted(0, 2.5, -2.5, 0).translated(0.5, -0.5);
      p.drawLine(QPointF(front.left() + 2.5, front.top() - 2.5),
                 QPointF(front.right() + 2.5, front.top() - 2.5));
      p.drawLine(QPointF(front.right() + 2.5, front.top() - 2.5),
                 QPointF(front.right() + 2.5, front.bottom() - 2.5));
      p.drawRoundedRect(front, 1.2, 1.2);
    }
    else
    {
      p.drawRoundedRect(glyph.adjusted(0.5, 0.5, -0.5, -0.5), 1.5, 1.5);
    }
    break;
  case Kind::Close:
  {
    const QRectF x = glyph.adjusted(0.5, 0.5, -0.5, -0.5);
    p.drawLine(x.topLeft(), x.bottomRight());
    p.drawLine(x.topRight(), x.bottomLeft());
    break;
  }
  }
}

// =====================================
// TitleBar
// =====================================

TitleBar::TitleBar(QWidget *window) : QWidget(window), p_window(window)
{
  this->setObjectName("hsdTitleBar");
  this->setFixedHeight(40);
  this->setAttribute(Qt::WA_StyledBackground, false);

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(8, 0, 6, 0);
  layout->setSpacing(2);

  this->p_menu_bar = new TitleMenuBar(this);
  this->p_menu_bar->setObjectName("hsdTitleMenuBar");
  this->p_menu_bar->setNativeMenuBar(false);
  this->p_menu_bar->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  layout->addWidget(this->p_menu_bar, 0, Qt::AlignVCenter);
  layout->addStretch(1);

  // extra controls (e.g. the viewer mode), left of the caption buttons
  this->p_trailing = new QHBoxLayout();
  this->p_trailing->setContentsMargins(0, 0, 10, 0);
  this->p_trailing->setSpacing(8);
  layout->addLayout(this->p_trailing);

  this->p_min = new WindowButton(WindowButton::Kind::Minimize, this);
  this->p_max = new WindowButton(WindowButton::Kind::Maximize, this);
  this->p_close = new WindowButton(WindowButton::Kind::Close, this);
  for (auto *button : {this->p_min, this->p_max, this->p_close})
  {
    button->setFixedSize(button->sizeHint());
    layout->addWidget(button, 0, Qt::AlignVCenter);
  }

  // project chip, centred on the bar (not in the layout: it is centred on the
  // window, not on the space the menu happens to leave)
  this->p_chip = new TitleChip(this);
  this->p_chip->on_clicked = [this](const QPoint &pos)
  {
    if (this->on_title_clicked)
      this->on_title_clicked(pos);
  };
  this->p_chip->on_layout = [this]() { this->place_title(); };

  this->update_style();

  // caption actions
  this->connect(this->p_min,
                &QAbstractButton::clicked,
                this,
                [this]() { this->p_window->showMinimized(); });

  this->connect(this->p_max,
                &QAbstractButton::clicked,
                this,
                [this]()
                {
                  if (this->p_window->isMaximized())
                    this->p_window->showNormal();
                  else
                    this->p_window->showMaximized();
                });

  this->connect(this->p_close,
                &QAbstractButton::clicked,
                this,
                [this]() { this->p_window->close(); });
}

void TitleBar::add_trailing_widget(QWidget *widget)
{
  widget->setParent(this);
  this->p_trailing->addWidget(widget, 0, Qt::AlignVCenter);
  this->place_title();
}

void TitleBar::update_style()
{
  const auto   &colors = HSD_CTX.app_settings.colors;
  const QString family = meta::qt::ui_font(13).family();

  static constexpr std::string_view css_template =
#include "hesiod/gui/styles/window_title_bar.css"
      ;

  const std::map<std::string, std::string> string_tokens = {
      {"FONT_FAMILY",
       family.isEmpty() ? ""
                        : std::format("font-family: \"{}\";", family.toStdString())}};

  const std::map<std::string, QColor> color_tokens = {
      {"INK", colors.text_primary},
      {"INK_DIM", mix_colors(colors.text_primary, colors.bg_deep, 0.35)},
      {"ACCENT", colors.accent}};

  QString css = render_stylesheet(css_template, string_tokens, color_tokens);
  this->setStyleSheet(css);

  if (this->p_menu_bar)
    this->p_menu_bar->update();

  if (this->p_chip)
    this->p_chip->update();

  if (this->p_min)
    this->p_min->update();

  if (this->p_max)
    this->p_max->update();

  if (this->p_close)
    this->p_close->update();
}

void TitleBar::begin_title_rename(const QString                       &current,
                                  std::function<void(const QString &)> commit)
{
  this->p_chip->begin_rename(current, std::move(commit));
}

void TitleBar::flash_saved() { this->p_chip->flash_saved(); }

bool TitleBar::is_caption_area(const QPoint &local_pos) const
{
  if (!this->rect().contains(local_pos))
    return false;

  QWidget *child = this->childAt(local_pos);
  if (!child)
    return true;

  // the menu bar is wider than its entries; its empty tail is caption too
  if (child == this->p_menu_bar)
  {
    const QPoint mb = this->p_menu_bar->mapFrom(this, local_pos);
    return this->p_menu_bar->actionAt(mb) == nullptr;
  }

  return false;
}

bool TitleBar::is_window_button(const QWidget *widget) const
{
  return widget == this->p_min || widget == this->p_max || widget == this->p_close;
}

void TitleBar::paintEvent(QPaintEvent *)
{
  QPainter p(this);
  p.fillRect(this->rect(), HSD_CTX.app_settings.colors.bg_deep);
}

void TitleBar::place_title()
{
  // nothing to place before the chip exists
  if (!this->p_chip)
    return;

  // free band between the menu entries and the right-hand controls
  const int left = this->p_menu_bar->geometry().right() + 16;
  int       right_edge = this->p_min->isVisible() ? this->p_min->geometry().left()
                                                  : this->width() - 6;
  if (this->p_trailing->count() > 0 && this->p_trailing->geometry().width() > 0)
    right_edge = std::min(right_edge, this->p_trailing->geometry().left());
  const int right = right_edge - 16;

  // The chip is as wide as its content (it says how wide, and keeps saying so
  // while that changes), up to what the band has to give.
  const int room = std::max(0, right - left);
  const int most = std::min(room, 460);
  this->p_chip->set_max_width(most);

  const int w = std::min(this->p_chip->natural_width(), most);
  const int h = TitleChip::kHeight;
  int       x = (this->width() - w) / 2;
  x = std::clamp(x, left, std::max(left, right - w));
  this->p_chip->setGeometry(x, (this->height() - h) / 2, w, h);
  this->p_chip->setVisible(room >= 120);
}

void TitleBar::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  this->place_title();
}

void TitleBar::set_maximized(bool new_state) { this->p_max->set_maximized(new_state); }

QWidget *TitleBar::title_chip() const { return this->p_chip; }

void TitleBar::set_project_title(const QString &name,
                                 const QString &path,
                                 bool           dirty,
                                 bool           saved)
{
  this->p_chip->set(name, path, dirty, saved);
  this->place_title();
}

void TitleBar::set_window_buttons_visible(bool new_state)
{
  for (auto *button : {this->p_min, this->p_max, this->p_close})
    button->setVisible(new_state);
  this->place_title();
}

} // namespace hesiod
