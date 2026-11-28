/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cmath>
#include <utility>

#include <QCursor>
#include <QEnterEvent>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QWheelEvent>

#include "hesiod/gui/widgets/node_widgets/node_palette/category_glyph.hpp"
#include "hesiod/gui/widgets/node_widgets/node_palette/node_palette_style.hpp"
#include "hesiod/gui/widgets/node_widgets/node_palette/palette_flyout_card.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"

namespace hesiod
{

namespace np = node_palette;

PaletteFlyoutCard::PaletteFlyoutCard(const NodePalettePalette            &style,
                                     std::function<void(const QString &)> on_pick,
                                     QWidget                             *owner)
    : QWidget(owner,
              Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint |
                  Qt::WindowDoesNotAcceptFocus),
      style(style), on_pick(std::move(on_pick)), appear_t(this, 0.0), geom_t(1.0),
      content_t(this, 1.0), highlight_y(this, 0.0), highlight_alpha(this, 0.0),
      scroll_y(this, 0.0), scrollbar_alpha(this, 0.0)
{
  this->setAttribute(Qt::WA_TranslucentBackground, true);
  this->setAttribute(Qt::WA_ShowWithoutActivating, true);
  this->setMouseTracking(true);
  this->setFocusPolicy(Qt::NoFocus);
  this->hide();

  // moving to another category: the card glides and changes size on the way
  this->geom_t.on_tick = [this]()
  {
    const double t = std::clamp(this->geom_t.value(), 0.0, 1.0);
    auto         lerp = [t](int a, int b)
    { return static_cast<int>(std::lround(a + (b - a) * t)); };
    this->setGeometry(lerp(this->geom_from.x(), this->geom_to.x()),
                      lerp(this->geom_from.y(), this->geom_to.y()),
                      lerp(this->geom_from.width(), this->geom_to.width()),
                      lerp(this->geom_from.height(), this->geom_to.height()));
    this->update();
  };

  this->appear_t.on_done = [this]()
  {
    if (!this->open_state && this->appear_t.value() <= np::hidden_below)
    {
      this->hide();
      this->active_row = -1;
      this->open_child_row = -1;
    }
  };

  this->sub_open_timer.setSingleShot(true);
  this->sub_open_timer.setInterval(motion::library_sub_open);
  this->connect(&this->sub_open_timer,
                &QTimer::timeout,
                this,
                [this]() { this->commit_pending_child(); });

  this->sub_close_timer.setSingleShot(true);
  this->sub_close_timer.setInterval(motion::library_sub_close);
  this->connect(&this->sub_close_timer,
                &QTimer::timeout,
                this,
                [this]()
                {
                  if (this->child && !this->child->contains_global(QCursor::pos()) &&
                      this->active_row != this->open_child_row)
                  {
                    this->open_child_row = -1;
                    this->child->dismiss();
                  }
                });

  this->scrollbar_timer.setSingleShot(true);
  this->scrollbar_timer.setInterval(motion::library_flyout_linger);
  this->connect(&this->scrollbar_timer,
                &QTimer::timeout,
                this,
                [this]()
                {
                  if (!this->hovered)
                    this->scrollbar_alpha.to(0.0,
                                             motion::library_flyout_thumb_out,
                                             motion::leave());
                });
}

PaletteFlyoutCard::~PaletteFlyoutCard()
{
  if (this->child)
    delete this->child;
}

bool PaletteFlyoutCard::animated() const
{
  return this->style.animations && this->style.animation_ms > 0 && motion::enabled();
}

qreal PaletteFlyoutCard::card_radius() const
{
  return this->style.corner_radius + np::card_radius_extra;
}

QRectF PaletteFlyoutCard::card_rect() const
{
  return QRectF(np::flyout_shadow,
                np::flyout_shadow - np::flyout_shadow_rise,
                this->width() - 2 * np::flyout_shadow,
                this->height() - 2 * np::flyout_shadow);
}

PaletteFlyoutCard *PaletteFlyoutCard::child_card() const { return this->child.data(); }

void PaletteFlyoutCard::commit_pending_child()
{
  const int row = this->pending_child_row;
  if (row < 0 || row >= static_cast<int>(this->rows.size()))
    return;
  const Row &r = this->rows[size_t(row)];
  if (!r.submenu)
    return;

  if (!this->child)
    this->child = new PaletteFlyoutCard(this->style, this->on_pick, this->parentWidget());

  this->open_child_row = row;
  const QRectF lr = this->list_rect();
  const QRectF c = this->card_rect();
  const qreal  row_cy = lr.top() + r.y - this->scroll_y.value() + r.h * 0.5;
  const QPoint anchor = this->mapToGlobal(
      QPoint(static_cast<int>(c.right()) + np::flyout_gap,
             static_cast<int>(std::lround(row_cy))));
  this->child->present(r.submenu, anchor, this->opened_left);
  this->update();
}

QRect PaletteFlyoutCard::compute_geometry(const QPoint &anchor_global, bool prefer_left)
{
  const int pad = this->padding();
  const int row_h = this->row_height();

  const QFontMetrics fm(this->font());
  int widest = this->current_node ? fm.horizontalAdvance(this->current_node->title) +
                                        np::flyout_title_room
                                  : np::flyout_min_width;
  for (const Row &r : this->rows)
  {
    if (r.separator)
      continue;
    const int extra = (r.submenu ? np::flyout_branch_room : np::flyout_leaf_room) +
                      np::flyout_icon + np::flyout_icon_gap;
    widest = std::max(widest, fm.horizontalAdvance(r.label) + extra + 2 * pad);
  }

  const int shadow = np::flyout_shadow;
  const int rise = np::flyout_shadow_rise;
  const int card_w = std::clamp(widest, np::flyout_min_width, np::flyout_max_width);

  const QScreen *screen = QGuiApplication::screenAt(anchor_global);
  if (!screen)
    screen = QGuiApplication::primaryScreen();
  const QRect avail = screen ? screen->availableGeometry() : QRect();

  // as tall as its rows, never taller than the cap or the screen: past that
  // it scrolls
  int list_cap = np::flyout_max_list_height;
  if (!avail.isNull())
    list_cap = std::min(list_cap, avail.height() - np::flyout_header_height);
  const int list_h = std::max(
      0,
      std::min(
          list_cap,
          std::max(row_h + 2 * pad, static_cast<int>(std::ceil(this->total_content_h)))));
  const int card_h = np::flyout_header_height + list_h;
  const int win_w = card_w + 2 * shadow;
  const int win_h = card_h + 2 * shadow;

  int x = anchor_global.x() - shadow;
  int y = anchor_global.y() - (shadow - rise) - np::flyout_header_height / 2;
  this->opened_left = false;

  // without a screen there is nothing to keep it inside
  if (avail.isNull())
    return QRect(x, y, win_w, win_h);

  if (prefer_left || x + win_w - shadow > avail.right() + 1)
  {
    const int left_x = anchor_global.x() - card_w - shadow - 2 * np::flyout_gap;
    if (left_x + shadow >= avail.left())
    {
      x = left_x;
      this->opened_left = true;
    }
    else
    {
      x = avail.right() + 1 - win_w + shadow;
    }
  }

  if (y + win_h - shadow > avail.bottom() + 1)
    y = avail.bottom() + 1 - win_h + shadow;
  if (y + (shadow - rise) < avail.top())
    y = avail.top() - (shadow - rise);

  return QRect(x, y, win_w, win_h);
}

bool PaletteFlyoutCard::contains_global(const QPoint &global_pos) const
{
  if (this->isVisible())
  {
    const QRectF card = this->card_rect().adjusted(-np::flyout_hit_margin,
                                                   -np::flyout_hit_margin,
                                                   np::flyout_hit_margin,
                                                   np::flyout_hit_margin);
    if (card.contains(QPointF(this->mapFromGlobal(global_pos))))
      return true;
  }
  return this->child && this->child->contains_global(global_pos);
}

void PaletteFlyoutCard::dismiss()
{
  this->sub_open_timer.stop();
  this->sub_close_timer.stop();
  this->pending_child_row = -1;
  this->open_child_row = -1;

  if (this->child)
    this->child->dismiss();

  if (!this->open_state)
    return;

  this->open_state = false;
  if (this->animated() && this->isVisible())
  {
    this->appear_t.to(0.0, motion::library_flyout_out, motion::leave());
  }
  else
  {
    this->appear_t.set(0.0);
    this->hide();
  }
}

void PaletteFlyoutCard::enterEvent(QEnterEvent *event)
{
  this->hovered = true;
  if (this->max_scroll() > 0.0)
    this->scrollbar_alpha.to(np::flyout_thumb_hovered,
                             motion::library_flyout_thumb_in,
                             motion::enter());
  QWidget::enterEvent(event);
}

bool PaletteFlyoutCard::is_open() const { return this->open_state; }

bool PaletteFlyoutCard::key(int key)
{
  if (!this->open_state)
    return false;

  // the deepest open card has the keys; Left there closes it
  if (this->child && this->child->is_open())
  {
    if (this->child->key(key))
      return true;
    if (key == Qt::Key_Left)
    {
      this->open_child_row = -1;
      this->child->dismiss();
      this->update();
      return true;
    }
    return false;
  }

  const bool has_row = this->active_row >= 0 &&
                       this->active_row < static_cast<int>(this->rows.size());

  switch (key)
  {
  case Qt::Key_Up:
    this->step_row(-1);
    return true;

  case Qt::Key_Down:
    this->step_row(1);
    return true;

  case Qt::Key_Return:
  case Qt::Key_Enter:
  case Qt::Key_Right:
  {
    if (!has_row)
      return false;

    const Row &r = this->rows[size_t(this->active_row)];
    if (r.submenu)
    {
      this->pending_child_row = this->active_row;
      this->commit_pending_child();
      if (this->child)
        this->child->step_row(1);
      return true;
    }
    if (key == Qt::Key_Right)
      return false;
    this->pick(r.label);
    return true;
  }

  default:
    return false;
  }
}

void PaletteFlyoutCard::leaveEvent(QEvent *event)
{
  this->hovered = false;
  this->scrollbar_timer.start();

  // the row stays lit while the pointer is on the sub-list it opened
  if (this->child && this->child->is_open() && this->open_child_row >= 0)
    this->set_active_row(this->open_child_row);
  else
    this->set_active_row(-1);

  QWidget::leaveEvent(event);
}

QRectF PaletteFlyoutCard::list_rect() const
{
  const QRectF c = this->card_rect();
  return QRectF(c.left(),
                c.top() + np::flyout_header_height,
                c.width(),
                std::max<qreal>(0.0, c.height() - np::flyout_header_height));
}

qreal PaletteFlyoutCard::max_scroll() const
{
  return std::max<qreal>(0.0, this->total_content_h - this->list_rect().height());
}

void PaletteFlyoutCard::mouseMoveEvent(QMouseEvent *event)
{
  const int row = this->row_at_pos(event->position());
  if (row != this->active_row)
    this->set_active_row(row);

  if (row >= 0 && row < static_cast<int>(this->rows.size()))
  {
    const Row &r = this->rows[size_t(row)];
    if (r.submenu)
    {
      this->sub_close_timer.stop();
      if (this->open_child_row != row)
      {
        this->pending_child_row = row;
        // from one sub-list to the next the open card moves at once
        if (this->child && this->child->is_open())
          this->commit_pending_child();
        else
          this->sub_open_timer.start();
      }
    }
    else if (this->child && this->child->is_open())
    {
      this->sub_open_timer.stop();
      this->sub_close_timer.start();
    }
  }

  QWidget::mouseMoveEvent(event);
}

void PaletteFlyoutCard::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::RightButton)
  {
    this->dismiss();
    event->accept();
    return;
  }

  if (event->button() != Qt::LeftButton)
    return;

  const int row = this->row_at_pos(event->position());
  if (row < 0 || row >= static_cast<int>(this->rows.size()))
    return;

  const Row &r = this->rows[size_t(row)];
  if (r.submenu)
  {
    this->pending_child_row = row;
    this->commit_pending_child();
  }
  else if (!r.separator && !r.label.isEmpty())
  {
    this->pick(r.label);
  }
  event->accept();
}

const PaletteTreeNode *PaletteFlyoutCard::node() const
{
  return this->current_node.get();
}

int PaletteFlyoutCard::padding() const { return std::max(0, this->style.flyout_padding); }

void PaletteFlyoutCard::paint_row(QPainter     &painter,
                                  const Row    &row,
                                  const QRectF &row_rect,
                                  bool          active) const
{
  const QColor &text = active ? this->style.text_active : this->style.text;
  qreal         right_edge = row_rect.right() - np::flyout_chevron_right;

  if (row.submenu)
  {
    QPen pen(text, np::flyout_chevron_pen);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const QPointF tip(row_rect.right() - np::flyout_chevron_right, row_rect.center().y());
    const qreal   arm = np::flyout_chevron_arm;
    painter.drawPolyline(QPolygonF() << QPointF(tip.x() - arm, tip.y() - arm) << tip
                                     << QPointF(tip.x() - arm, tip.y() + arm));
    right_edge -= np::flyout_chevron_room;

    if (row.count > 0)
    {
      QFont count_font = this->font();
      if (count_font.pointSizeF() > 0)
        count_font.setPointSizeF(count_font.pointSizeF() * np::flyout_count_font);
      const QFontMetrics count_fm(count_font);
      const QString      count = QString::number(row.count);
      const int          cw = count_fm.horizontalAdvance(count);

      painter.setFont(count_font);
      painter.setPen(with_alpha(this->style.text, np::flyout_count_ink));
      painter.drawText(QRectF(right_edge - cw - np::flyout_count_gap,
                              row_rect.top(),
                              cw + np::flyout_count_gap,
                              row_rect.height()),
                       Qt::AlignRight | Qt::AlignVCenter,
                       count);
      right_edge -= cw + np::flyout_count_room;
    }
  }

  // the glyph ahead of the name carries the category color held back, like
  // the rail's glyphs, and comes forward on the row under the pointer
  const QColor accent = this->current_node ? this->current_node->accent
                                           : this->style.text;
  const qreal  pull = std::clamp<qreal>(this->style.accent_strength *
                                           (active ? 1.0 : np::flyout_rest_ink),
                                       0.0,
                                       1.0);
  const QColor ink = mix_colors(text, accent, pull);
  const QRectF icon_box(row_rect.left() + np::flyout_icon_left,
                        std::round(row_rect.center().y() - np::flyout_icon * 0.5),
                        np::flyout_icon,
                        np::flyout_icon);
  // a sub-list wears its own glyph, a node its category's
  const QString glyph_of = row.submenu
                               ? row.submenu->title
                               : (this->current_node ? this->current_node->category
                                                     : QString());
  draw_node_glyph(painter, icon_box, ink, row.icon, glyph_of);

  const QFontMetrics fm(this->font());
  const qreal        text_left = icon_box.right() + np::flyout_text_gap;
  const qreal text_w = std::max<qreal>(np::flyout_text_min, right_edge - text_left);
  painter.setFont(this->font());
  painter.setPen(text);
  painter.drawText(QRectF(text_left, row_rect.top(), text_w, row_rect.height()),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   fm.elidedText(row.label, Qt::ElideRight, static_cast<int>(text_w)));
}

void PaletteFlyoutCard::paint_shadow(QPainter     &painter,
                                     const QRectF &card,
                                     qreal         alpha) const
{
  painter.setPen(Qt::NoPen);
  for (int i = np::flyout_shadow_layers; i >= 1; --i)
  {
    const qreal spread = i * np::flyout_shadow_spread;
    const qreal drop = i * np::flyout_shadow_drop;
    const qreal radius = this->card_radius() + spread * np::flyout_shadow_round;
    const int   a = static_cast<int>(
        std::lround((np::flyout_shadow_alpha - i * np::flyout_shadow_fade) * alpha));
    painter.setBrush(QColor(0, 0, 0, std::clamp(a, 0, 255)));
    painter.drawRoundedRect(card.adjusted(-spread, -spread + drop, spread, spread + drop),
                            radius,
                            radius);
  }
}

void PaletteFlyoutCard::paintEvent(QPaintEvent *)
{
  const qreal shown = std::clamp(this->appear_t.value(), 0.0, 1.0);
  const qreal alpha = std::min<qreal>(1.0, shown * np::flyout_opacity_lead);
  if (alpha <= np::hidden_below)
    return;

  QPainter painter(this);
  painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
  painter.setOpacity(alpha);

  // The card comes in over a short travel, in whole pixels and at its own
  // size: its rows are text, and scaled by a few percent or moved by a
  // fraction of a pixel every glyph is laid out anew on every frame.
  const QRectF card = this->card_rect();
  const qreal  dx = std::round(
      (this->opened_left ? np::flyout_travel : -np::flyout_travel) * (1.0 - shown));
  const qreal radius = this->card_radius();
  painter.save();
  painter.translate(dx, 0.0);

  this->paint_shadow(painter, card, alpha);

  QPainterPath card_path;
  card_path.addRoundedRect(card, radius, radius);
  painter.fillPath(card_path, this->style.flyout_bg);

  painter.save();
  painter.setClipPath(card_path);

  const qreal swap = std::clamp(this->content_t.value(), 0.0, 1.0);
  painter.setOpacity(alpha *
                     (np::flyout_swap_from + (1.0 - np::flyout_swap_from) * swap));

  // the category's name, its color as a dot, and a rule below
  if (this->current_node)
  {
    const QRectF header(card.left(), card.top(), card.width(), np::flyout_header_height);
    painter.setPen(Qt::NoPen);
    painter.setBrush(with_alpha(this->current_node->accent, np::flyout_dot_ink));
    painter.drawEllipse(QPointF(header.left() + np::flyout_dot_left, header.center().y()),
                        np::flyout_dot,
                        np::flyout_dot);

    QFont title_font = this->font();
    if (title_font.pointSizeF() > 0)
      title_font.setPointSizeF(title_font.pointSizeF() * np::flyout_title_font);
    title_font.setWeight(QFont::DemiBold);
    painter.setFont(title_font);
    painter.setPen(this->style.text_active);
    painter.drawText(
        header.adjusted(np::flyout_title_left, 0.0, -np::flyout_title_right, 0.0),
        Qt::AlignLeft | Qt::AlignVCenter,
        this->current_node->title);

    painter.fillRect(QRectF(card.left() + np::flyout_title_rule_inset,
                            header.bottom() - 1.0,
                            card.width() - 2.0 * np::flyout_title_rule_inset,
                            1.0),
                     with_alpha(this->style.flyout_border, np::flyout_title_rule_ink));
  }

  const QRectF lr = this->list_rect();
  const qreal  pad = this->padding();
  const qreal  sc = this->scroll_y.value();
  painter.save();
  painter.setClipRect(lr);

  // one plate, gliding from row to row
  const qreal plate_alpha = std::clamp(this->highlight_alpha.value(), 0.0, 1.0);
  if (plate_alpha > np::hidden_below)
  {
    const QRectF plate(lr.left() + pad,
                       lr.top() + this->highlight_y.value() - sc + np::flyout_plate_inset,
                       lr.width() - 2.0 * pad,
                       this->row_height() - 2.0 * np::flyout_plate_inset);
    if (plate.bottom() >= lr.top() && plate.top() <= lr.bottom())
    {
      QColor fill = this->style.flyout_hover;
      fill.setAlphaF(fill.alphaF() * plate_alpha);
      painter.setPen(Qt::NoPen);
      painter.setBrush(fill);
      painter.drawRoundedRect(plate, np::flyout_plate_radius, np::flyout_plate_radius);
    }
  }

  for (size_t i = 0; i < this->rows.size(); ++i)
  {
    const Row  &r = this->rows[i];
    const qreal ry = lr.top() + r.y - sc;
    if (ry + r.h < lr.top() || ry > lr.bottom())
      continue;

    if (r.separator)
    {
      painter.fillRect(QRectF(lr.left() + np::flyout_rule_inset,
                              ry + r.h * 0.5,
                              lr.width() - 2.0 * np::flyout_rule_inset,
                              1.0),
                       with_alpha(this->style.flyout_border, np::flyout_rule_ink));
      continue;
    }

    const bool active = static_cast<int>(i) == this->active_row ||
                        static_cast<int>(i) == this->open_child_row;
    this->paint_row(painter,
                    r,
                    QRectF(lr.left() + pad, ry, lr.width() - 2.0 * pad, r.h),
                    active);
  }

  painter.restore(); // list clip

  // a thin bar, only for a category taller than the card
  const qreal max_sc = this->max_scroll();
  const qreal bar_alpha = std::clamp(this->scrollbar_alpha.value(), 0.0, 1.0);
  if (max_sc > 0.0 && bar_alpha > np::hidden_below && this->total_content_h > 0.0)
  {
    const qreal track_h = lr.height() - 2.0 * np::flyout_thumb_inset;
    const qreal thumb_h = std::max<qreal>(np::flyout_thumb_min,
                                          track_h * lr.height() / this->total_content_h);
    const qreal along = std::clamp(sc / max_sc, 0.0, 1.0);
    const qreal thumb_y = lr.top() + np::flyout_thumb_inset + (track_h - thumb_h) * along;
    const qreal w = np::flyout_thumb_width;

    painter.setPen(Qt::NoPen);
    painter.setBrush(with_alpha(this->style.text, np::flyout_thumb_ink * bar_alpha));
    painter.drawRoundedRect(
        QRectF(card.right() - np::flyout_thumb_edge, thumb_y, w, thumb_h),
        w * 0.5,
        w * 0.5);
  }

  painter.restore(); // card clip

  painter.setPen(QPen(this->style.flyout_border, 1.0));
  painter.setBrush(Qt::NoBrush);
  painter.drawRoundedRect(card.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);

  painter.restore(); // travel
}

void PaletteFlyoutCard::pick(const QString &node_type)
{
  if (this->on_pick && !node_type.isEmpty())
    this->on_pick(node_type);
}

void PaletteFlyoutCard::present(const std::shared_ptr<PaletteTreeNode> &node,
                                const QPoint                           &anchor_global,
                                bool                                    prefer_left)
{
  if (!node)
    return;

  this->sub_open_timer.stop();
  this->sub_close_timer.stop();
  this->pending_child_row = -1;

  const bool was_open = this->open_state && this->isVisible() &&
                        this->appear_t.value() > np::flyout_shown;
  const bool same_node = this->current_node == node;

  if (this->child && !same_node)
  {
    this->open_child_row = -1;
    this->child->dismiss();
  }

  this->current_node = node;
  this->rebuild_rows();

  const QRect target = this->compute_geometry(anchor_global, prefer_left);
  this->open_state = true;
  this->active_row = -1;
  this->highlight_alpha.set(0.0);
  this->scroll_y.set(0.0);

  if (was_open && this->animated())
  {
    // the open card glides to the new category and takes its size on the way
    this->geom_from = this->geometry();
    this->geom_to = target;
    this->geom_t.set(0.0);
    this->geom_t.to(1.0, motion::library_flyout_glide, motion::enter());

    if (!same_node)
    {
      this->content_t.set(0.0);
      this->content_t.to(1.0, motion::library_flyout_swap, motion::enter());
    }
    this->appear_t.to(1.0, motion::library_flyout_settle, motion::enter());
  }
  else
  {
    this->geom_from = target;
    this->geom_to = target;
    this->geom_t.set(1.0);
    this->setGeometry(target);
    this->content_t.set(1.0);
    this->show();

    if (this->animated())
    {
      this->appear_t.set(0.0);
      this->appear_t.to(1.0, motion::library_flyout_in, motion::enter());
    }
    else
    {
      this->appear_t.set(1.0);
    }
  }

  this->raise();
  this->update();
}

void PaletteFlyoutCard::rebuild_rows()
{
  this->rows.clear();
  const qreal row_h = this->row_height();
  qreal       y = this->padding();

  if (!this->current_node)
  {
    this->total_content_h = 0.0;
    return;
  }

  for (const QString &leaf : this->current_node->leaves)
  {
    this->rows.push_back({false, leaf, leaf, nullptr, 0, y, row_h});
    y += row_h;
  }

  if (!this->current_node->leaves.empty() && !this->current_node->children.empty())
  {
    this->rows.push_back(
        {true, QString(), QString(), nullptr, 0, y, qreal(np::flyout_separator)});
    y += np::flyout_separator;
  }

  for (const auto &sub : this->current_node->children)
  {
    if (!sub)
      continue;
    this->rows.push_back(
        {false, sub->title, sub->path, sub, sub->total_leaves(), y, row_h});
    y += row_h;
  }

  this->total_content_h = y + this->padding();
}

int PaletteFlyoutCard::row_at_pos(const QPointF &pos) const
{
  const QRectF lr = this->list_rect();
  if (!lr.contains(pos))
    return -1;
  const qreal y = pos.y() - lr.top() + this->scroll_y.value();
  for (size_t i = 0; i < this->rows.size(); ++i)
  {
    const Row &r = this->rows[i];
    if (!r.separator && y >= r.y && y < r.y + r.h)
      return static_cast<int>(i);
  }
  return -1;
}

int PaletteFlyoutCard::row_height() const
{
  return std::max(np::flyout_icon, this->style.flyout_row_height);
}

void PaletteFlyoutCard::scroll_to_row(int row)
{
  if (row < 0 || row >= static_cast<int>(this->rows.size()))
    return;

  const Row  &r = this->rows[size_t(row)];
  const qreal view_h = this->list_rect().height();
  const qreal from = this->scroll_y.target();
  qreal       to = from;
  if (r.y < from)
    to = r.y - this->padding();
  else if (r.y + r.h > from + view_h)
    to = r.y + r.h + this->padding() - view_h;

  to = std::clamp(to, 0.0, this->max_scroll());
  if (to != from)
    this->scroll_y.to(to, motion::library_flyout_scroll, motion::enter());
}

bool PaletteFlyoutCard::scrolls() const { return this->max_scroll() > 0.0; }

void PaletteFlyoutCard::set_active_row(int row)
{
  this->active_row = row;
  this->setCursor(row >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);

  if (row < 0 || row >= static_cast<int>(this->rows.size()) ||
      this->rows[size_t(row)].separator)
  {
    this->highlight_alpha.to(0.0, motion::hover_out, motion::leave());
    this->update();
    return;
  }

  // the plate glides from the row it is on; arriving from nothing it starts
  // on the row
  const qreal target_y = this->rows[size_t(row)].y;
  if (!this->animated() || this->highlight_alpha.value() < np::hidden_below)
    this->highlight_y.set(target_y);
  else
    this->highlight_y.to(target_y, motion::library_flyout_plate, motion::enter());

  this->highlight_alpha.to(1.0, motion::hover_in, motion::enter());
  this->update();
}

void PaletteFlyoutCard::set_style(const NodePalettePalette &new_style)
{
  this->style = new_style;
  if (this->child)
    this->child->set_style(new_style);
  this->rebuild_rows();
  this->update();
}

void PaletteFlyoutCard::step_row(int direction)
{
  const int n = static_cast<int>(this->rows.size());
  if (n == 0)
    return;

  int row = this->active_row;
  if (row < 0)
    row = direction > 0 ? -1 : n;

  for (int next = row + direction; next >= 0 && next < n; next += direction)
    if (!this->rows[size_t(next)].separator)
    {
      this->set_active_row(next);
      this->scroll_to_row(next);
      return;
    }
}

void PaletteFlyoutCard::wheelEvent(QWheelEvent *event)
{
  event->accept();

  const qreal max_sc = this->max_scroll();
  if (max_sc <= 0.0)
    return;

  qreal delta = 0.0;
  if (!event->pixelDelta().isNull())
    delta = -event->pixelDelta().y();
  else
    delta = -(event->angleDelta().y() / qreal(QWheelEvent::DefaultDeltasPerStep)) *
            (this->row_height() * np::flyout_wheel_rows);

  const qreal target = std::clamp(this->scroll_y.target() + delta, 0.0, max_sc);
  this->scroll_y.to(target, motion::library_flyout_scroll, motion::enter());
  this->scrollbar_alpha.to(1.0, motion::library_flyout_thumb_wake, motion::enter());
  this->scrollbar_timer.start();

  const int row = this->row_at_pos(this->mapFromGlobal(QCursor::pos()));
  if (row != this->active_row)
    this->set_active_row(row);
}

} // namespace hesiod
