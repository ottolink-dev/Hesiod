/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

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
#include <QVariantAnimation>

#include "meta_qt/ui/theme.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/gui/widgets/window_chrome.hpp"

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
  this->update();
  QAbstractButton::enterEvent(event);
}

void WindowButton::leaveEvent(QEvent *event)
{
  this->hovered = false;
  this->update();
  QAbstractButton::leaveEvent(event);
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

  const bool   down = this->isDown();
  const QRectF box = QRectF(this->rect()).adjusted(2, 2, -2, -2);
  const QColor close_red("#c8463d");

  // hover plate
  if (this->hovered || down)
  {
    QColor plate;
    if (this->kind == Kind::Close)
      plate = down ? close_red.darker(115) : close_red;
    else
      plate = mix_colors(colors.bg_deep, colors.text_primary, down ? 0.10 : 0.07);

    p.setPen(Qt::NoPen);
    p.setBrush(plate);
    p.drawRoundedRect(box, 6, 6);
  }

  // glyph colour: close is tinted red at rest (as a cue for the destructive
  // one) and turns light on its red plate
  QColor ink = mix_colors(colors.text_primary, colors.bg_deep, 0.30);
  if (this->kind == Kind::Close)
    ink = (this->hovered || down) ? QColor("#f4f4f5") : close_red.lighter(115);
  else if (this->hovered)
    ink = colors.text_primary;

  // all glyphs share one 10x10 box snapped to the pixel grid, so the three
  // buttons read as a set
  const qreal   g = 10.0;
  const QPointF c(std::round(this->width() / 2.0), std::round(this->height() / 2.0));
  const QRectF  glyph(c.x() - g / 2, c.y() - g / 2, g, g);

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
// TitleChip
// =====================================

// The project name in the title bar: shows the name, a "Not saved" hint and an
// unsaved-changes dot, opens the project menu on click and edits the name in
// place on request.
class TitleChip final : public QWidget
{
public:
  explicit TitleChip(QWidget *parent) : QWidget(parent)
  {
    this->setCursor(Qt::PointingHandCursor);
    this->setAttribute(Qt::WA_Hover);

    // A plain QWidget picks up the application stylesheet's background, filled
    // as a square behind the rounded chip: opt out, the chip paints itself.
    this->setObjectName("hsdTitleChip");
    this->setAttribute(Qt::WA_NoSystemBackground);
    this->setAutoFillBackground(false);
    this->setStyleSheet(
        "QWidget#hsdTitleChip { background: transparent; border: none; }");

    this->flip = new QVariantAnimation(this);
    this->flip->setDuration(280);
    this->flip->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(this->flip,
                     &QVariantAnimation::valueChanged,
                     this,
                     [this](const QVariant &v)
                     {
                       this->open_t = v.toReal();
                       this->update();
                     });

    this->editor = new QLineEdit(this);
    this->editor->setObjectName("hsdTitleEditor");
    this->editor->setAlignment(Qt::AlignCenter);
    this->editor->setFrame(false);
    this->editor->hide();
    this->editor->installEventFilter(this);

    QObject::connect(this->editor,
                     &QLineEdit::returnPressed,
                     this,
                     [this]() { this->finish_rename(true); });
  }

  void set(const QString &new_name, const QString &new_path, bool new_dirty, bool saved)
  {
    this->name = new_name.isEmpty() ? QString("Untitled") : new_name;
    this->hint = saved ? QString() : QString("Not saved");
    this->dirty = new_dirty;
    this->setToolTip(saved ? new_path + "\n\nClick for project actions (F2 renames)"
                           : QString("This project has no file yet.\n\nClick for project "
                                     "actions (F2 renames)"));
    this->update();
  }

  // width the chip would like for its content
  int natural_width() const
  {
    const QFontMetrics fm(this->name_font());
    const QFontMetrics fh(this->hint_font());
    int w = 28 + fm.horizontalAdvance(this->name) + 28 /* divider + chevron */;
    if (!this->hint.isEmpty())
      w += 10 + fh.horizontalAdvance(this->hint);
    if (this->dirty)
      w += 12;
    return w;
  }

  bool renaming() const { return this->editor->isVisible(); }

  void begin_rename(const QString &current, std::function<void(const QString &)> commit)
  {
    this->on_commit = std::move(commit);
    this->editor->setGeometry(this->rect().adjusted(10, 3, -10, -3));
    this->editor->setText(current);
    this->editor->selectAll();
    this->editor->show();
    this->editor->setFocus(Qt::OtherFocusReason);
    this->update();
  }

  std::function<void(const QPoint &)> on_clicked;

protected:
  bool eventFilter(QObject *watched, QEvent *event) override
  {
    if (watched == this->editor)
    {
      if (event->type() == QEvent::KeyPress &&
          static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape)
      {
        this->finish_rename(false);
        return true;
      }
      if (event->type() == QEvent::FocusOut && this->editor->isVisible())
        this->finish_rename(false); // clicking away cancels, Enter commits
    }
    return QWidget::eventFilter(watched, event);
  }

  void resizeEvent(QResizeEvent *event) override
  {
    QWidget::resizeEvent(event);
    if (this->editor->isVisible())
      this->editor->setGeometry(this->rect().adjusted(10, 3, -10, -3));
  }

  // The menu opens on press, like a menu bar item: waiting for the release,
  // or for a possible double-click, made it feel as if it lagged behind.
  // Rename is the menu's first item (and F2).
  void mousePressEvent(QMouseEvent *event) override
  {
    if (event->button() != Qt::LeftButton || this->renaming() || !this->on_clicked)
      return QWidget::mousePressEvent(event);

    event->accept();

    // A click on the chip while its menu is open only closes the menu: the
    // menu is built with setNoReplayFor(chip) (see TitleBar::title_chip), so
    // Qt does not replay that press here and open it straight again.
    this->pressed = true;
    this->animate_open(true);
    this->repaint();
    this->on_clicked(this->mapToGlobal(QPoint(this->width() / 2, this->height() + 4)));
    this->pressed = false;
    this->animate_open(false);
    this->update();
  }

  void mouseReleaseEvent(QMouseEvent *event) override
  {
    this->pressed = false;
    this->update();
    QWidget::mouseReleaseEvent(event);
  }

  // a quick second click is a click too (it reopens the menu the first one
  // closed), not a rename
  void mouseDoubleClickEvent(QMouseEvent *event) override
  {
    this->mousePressEvent(event);
  }

  void paintEvent(QPaintEvent *) override
  {
    const auto &colors = HSD_CTX.app_settings.colors;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const bool   hover = this->underMouse() || this->renaming();
    const QRectF box = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    QColor fill = colors.bg_primary;
    if (this->pressed)
      fill = mix_colors(colors.bg_primary, colors.text_primary, 0.10);
    else if (hover)
      fill = mix_colors(colors.bg_primary, colors.text_primary, 0.05);

    p.setPen(QPen(hover ? mix_colors(colors.bg_primary, colors.border, 0.75)
                        : panel_border_color(),
                  1));
    p.setBrush(fill);
    p.drawRoundedRect(box, 7, 7);

    if (this->renaming())
      return;

    // content centred as a group: [name] [hint] [dot]; chevron at the right
    const QFontMetrics fm(this->name_font());
    const QFontMetrics fh(this->hint_font());

    const int chevron_space = 28;
    const int avail = this->width() - 24 - chevron_space;
    const int hint_w = this->hint.isEmpty() ? 0 : 10 + fh.horizontalAdvance(this->hint);
    const int dot_w = this->dirty ? 12 : 0;

    const QString shown = fm.elidedText(this->name,
                                        Qt::ElideMiddle,
                                        std::max(20, avail - hint_w - dot_w));
    const int     name_w = fm.horizontalAdvance(shown);
    const int     group = name_w + hint_w + dot_w;
    int           x = std::max(12, (this->width() - chevron_space - group) / 2 + 6);

    p.setFont(this->name_font());
    p.setPen(colors.text_primary);
    p.drawText(QRect(x, 0, name_w + 2, this->height()), Qt::AlignVCenter, shown);
    x += name_w;

    if (!this->hint.isEmpty())
    {
      p.setFont(this->hint_font());
      p.setPen(mix_colors(colors.bg_primary, colors.text_primary, 0.50));
      p.drawText(QRect(x + 10, 0, hint_w, this->height()), Qt::AlignVCenter, this->hint);
      x += hint_w;
    }

    if (this->dirty)
    {
      p.setPen(Qt::NoPen);
      p.setBrush(colors.accent);
      p.drawEllipse(QPointF(x + 8, this->height() / 2.0), 3.0, 3.0);
    }

    // the menu affordance: a hairline divider, then a chevron that turns to
    // point up while the menu is open
    {
      const qreal divider_x = this->width() - 26.5;
      p.setPen(
          QPen(mix_colors(colors.bg_primary, colors.border, hover ? 0.55 : 0.35), 1));
      p.drawLine(QPointF(divider_x, 7), QPointF(divider_x, this->height() - 7));

      QPen pen(mix_colors(colors.bg_primary, colors.text_primary, hover ? 0.9 : 0.6),
               1.6);
      pen.setCapStyle(Qt::RoundCap);
      pen.setJoinStyle(Qt::RoundJoin);
      p.setPen(pen);
      p.setBrush(Qt::NoBrush);

      p.save();
      p.translate(this->width() - 13.5, this->height() / 2.0);
      p.rotate(180.0 * this->open_t);
      QPainterPath chevron;
      chevron.moveTo(-4.0, -2.0);
      chevron.lineTo(0.0, 2.0);
      chevron.lineTo(4.0, -2.0);
      p.drawPath(chevron);
      p.restore();
    }
  }

private:
  QFont name_font() const { return meta::qt::ui_font(12); }
  QFont hint_font() const { return meta::qt::ui_font(11); }

  void finish_rename(bool commit)
  {
    if (!this->editor->isVisible())
      return;

    const QString text = this->editor->text().trimmed();
    this->editor->hide();
    this->update();

    auto callback = std::move(this->on_commit);
    this->on_commit = nullptr;
    if (commit && callback)
      callback(text);
  }

  QString    name = "Untitled", hint;
  bool       dirty = false;
  bool       pressed = false;
  QLineEdit *editor = nullptr;

  // chevron turn (0: down, 1: up while the menu is open), and the menu's
  // state for mousePressEvent (see there)
  QVariantAnimation *flip = nullptr;
  qreal              open_t = 0.0;

  void animate_open(bool open)
  {
    this->flip->stop();
    this->flip->setDuration(HSD_CTX.app_settings.interface.enable_ui_animations ? 280
                                                                                : 1);
    this->flip->setStartValue(this->open_t);
    this->flip->setEndValue(open ? 1.0 : 0.0);
    this->flip->start();
  }

  std::function<void(const QString &)> on_commit;
};

// =====================================
// TitleBar
// =====================================

TitleBar::TitleBar(QWidget *window) : QWidget(window), p_window(window)
{
  const auto &colors = HSD_CTX.app_settings.colors;

  this->setObjectName("hsdTitleBar");
  this->setFixedHeight(40);
  this->setAttribute(Qt::WA_StyledBackground, false);

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(8, 0, 6, 0);
  layout->setSpacing(2);

  this->p_menu_bar = new QMenuBar(this);
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

  const QString family = meta::qt::ui_font(13).family();
  const QString ink = colors.text_primary.name();
  const QString ink_dim = mix_colors(colors.text_primary, colors.bg_deep, 0.35).name();
  const QString hover = mix_colors(colors.bg_deep, colors.text_primary, 0.08).name();
  const QString pressed = mix_colors(colors.bg_deep, colors.text_primary, 0.13).name();

  QString css = QString(R"(
    QWidget#hsdTitleBar { background: transparent; }
    QMenuBar#hsdTitleMenuBar {
      background: transparent; border: none; padding: 0; margin: 0;
      color: INK_DIM; spacing: 2px; FONT_FAMILY font-size: 13px; }
    QMenuBar#hsdTitleMenuBar::item {
      background: transparent; color: INK_DIM; padding: 5px 10px; border-radius: 6px; }
    QMenuBar#hsdTitleMenuBar::item:selected { background: HOVER; color: INK; }
    QMenuBar#hsdTitleMenuBar::item:pressed { background: PRESSED; color: INK; }
    QLineEdit#hsdTitleEditor {
      background: transparent; border: none; color: INK; FONT_FAMILY font-size: 12px;
      selection-background-color: ACCENT; }
  )");
  css.replace("FONT_FAMILY",
              family.isEmpty() ? QString() : QString("font-family: \"%1\";").arg(family));
  css.replace("INK_DIM", ink_dim);
  css.replace("INK", ink);
  css.replace("HOVER", hover);
  css.replace("PRESSED", pressed);
  css.replace("ACCENT", colors.accent.name());
  this->setStyleSheet(css);

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

void TitleBar::begin_title_rename(const QString                       &current,
                                  std::function<void(const QString &)> commit)
{
  this->p_chip->begin_rename(current, std::move(commit));
}

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
  // free band between the menu entries and the right-hand controls
  const int left = this->p_menu_bar->geometry().right() + 16;
  int       right_edge = this->p_min->isVisible() ? this->p_min->geometry().left()
                                                  : this->width() - 6;
  if (this->p_trailing->count() > 0 && this->p_trailing->geometry().width() > 0)
    right_edge = std::min(right_edge, this->p_trailing->geometry().left());
  const int right = right_edge - 16;

  // a fixed minimum keeps short names ("island") from shrinking the chip into
  // a pill that no longer reads as the title
  const int room = std::max(0, right - left);
  const int natural = this->p_chip->natural_width();
  const int w = std::min(std::max(natural, 240), std::min(room, 460));
  const int h = 26;
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
