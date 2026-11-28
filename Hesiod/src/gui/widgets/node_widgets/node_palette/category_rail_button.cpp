/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include <QEnterEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QPainter>

#include "hesiod/gui/widgets/node_widgets/node_palette/category_glyph.hpp"
#include "hesiod/gui/widgets/node_widgets/node_palette/category_rail_button.hpp"
#include "hesiod/gui/widgets/node_widgets/node_palette/node_palette_style.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"

namespace hesiod
{

namespace np = node_palette;

CategoryRailButton::CategoryRailButton(const QString            &category,
                                       const QColor             &accent,
                                       const NodePalettePalette &style,
                                       QWidget                  *parent)
    : QAbstractButton(parent), category(category), accent(accent), style(style),
      heat_t(this, 0.0), open_t(this, 0.0)
{
  this->setText(category);
  this->setCursor(Qt::PointingHandCursor);
  this->setFocusPolicy(Qt::TabFocus);
  this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  this->setAttribute(Qt::WA_Hover, true);

  this->surface = this->style.surface;

  this->surface_animation.setEasingCurve(motion::enter());
  this->connect(&this->surface_animation,
                &QVariantAnimation::valueChanged,
                this,
                [this](const QVariant &value)
                {
                  this->surface = value.value<QColor>();
                  this->update();
                });
  this->connect(&this->surface_animation,
                &QVariantAnimation::finished,
                this,
                [this]()
                {
                  this->surface = this->target_surface();
                  this->update();
                });
}

QColor CategoryRailButton::current_surface() const { return this->surface; }

void CategoryRailButton::enterEvent(QEnterEvent *event)
{
  const bool was_hovering = this->hovering;
  this->hovering = true;
  this->retarget_surface();
  if (!was_hovering)
    Q_EMIT this->hovered();
  QAbstractButton::enterEvent(event);
}

bool CategoryRailButton::event(QEvent *event)
{
  switch (event->type())
  {
  case QEvent::HoverEnter:
  {
    const bool was_hovering = this->hovering;
    this->hovering = true;
    this->retarget_surface();
    if (!was_hovering)
      Q_EMIT this->hovered();
    break;
  }
  case QEvent::HoverLeave:
  {
    this->hovering = false;
    this->retarget_surface();
    break;
  }
  default:
    break;
  }
  return QAbstractButton::event(event);
}

double CategoryRailButton::get_reveal() const { return this->reveal; }

bool CategoryRailButton::is_open() const { return this->open; }

void CategoryRailButton::keyPressEvent(QKeyEvent *event)
{
  switch (event->key())
  {
  case Qt::Key_Right:
    // into the category: its flyout takes the keyboard from here
    if (!this->open)
      this->click();
    event->accept();
    return;

  case Qt::Key_Left:
    if (this->open)
      this->click();
    event->accept();
    return;

  default:
    break;
  }
  QAbstractButton::keyPressEvent(event);
}

void CategoryRailButton::leaveEvent(QEvent *event)
{
  this->hovering = false;
  this->retarget_surface();
  QAbstractButton::leaveEvent(event);
}

void CategoryRailButton::paintEvent(QPaintEvent *)
{
  if (!this->open && this->hovering)
  {
    const QPoint global_mouse = QCursor::pos();
    if (!this->rect().contains(this->mapFromGlobal(global_mouse)))
    {
      this->hovering = false;
      this->retarget_surface();
    }
  }

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  const QRectF box = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  const double radius = std::min<double>(this->style.corner_radius, box.height() / 2.0);
  const double heat = std::clamp(this->heat_t.value(), 0.0, 1.0);
  const double open_amt = std::clamp(this->open_t.value(), 0.0, 1.0);

  // At rest the surface is the card's own color, so a row is just its glyph
  // and its name; a gray plate appears only under the pointer and for the
  // category whose flyout is open.
  painter.setPen(Qt::NoPen);
  painter.setBrush(this->surface);
  painter.drawRoundedRect(box, radius, radius);

  const double icon = this->style.icon_size;
  const double pad = this->style.rail_padding;
  const double cy = box.center().y();

  // The glyph is in the same place folded and unfolded: the middle of the
  // square cell a folded row is. Folding then moves nothing inside the rail,
  // the edge simply comes in over the names.
  const double x_glyph = box.left() + (this->style.button_height - icon) * 0.5;
  const QRectF glyph_box(x_glyph, cy - icon * 0.5, icon, icon);

  // the accent only ever touches the glyph and a short bar, never the surface:
  // a full-width category color is what makes the old tree hard to read
  if (open_amt > np::hidden_below)
  {
    const double bar_h = np::open_bar_height * open_amt;
    painter.setBrush(with_alpha(this->accent, open_amt));
    painter.drawRoundedRect(QRectF(box.left() + np::open_bar_left,
                                   cy - bar_h * 0.5,
                                   np::open_bar_width,
                                   bar_h),
                            np::open_bar_width * 0.5,
                            np::open_bar_width * 0.5);
  }

  // the category color, held back at rest: eighteen full colors in a column
  // are a rainbow, and the one under the hand should be the one that stands out
  const double strength = this->style.accent_strength *
                          (np::glyph_rest_strength +
                           (1.0 - np::glyph_rest_strength) * heat);
  const double inset = icon * np::glyph_inset;
  draw_category_glyph(painter,
                      glyph_box.adjusted(inset, inset, -inset, -inset),
                      mix_colors(this->style.text, this->accent, strength),
                      category_glyph_shape(this->category));

  // the name fades cleanly, without re-eliding against the animated width
  // every frame
  const double label_alpha = np::smoothstep(np::label_fade_from,
                                            np::label_fade_to,
                                            this->reveal);
  if (label_alpha <= np::hidden_below)
    return;

  painter.setOpacity(label_alpha);
  painter.setFont(this->font());
  painter.setPen(mix_colors(this->style.text, this->style.text_active, heat));

  const QRectF text_clip(
      glyph_box.right() + pad,
      box.top(),
      std::max(0.0, box.right() - glyph_box.right() - 2.0 * pad - np::chevron_room),
      box.height());

  if (text_clip.width() > 2.0)
  {
    painter.save();
    painter.setClipRect(text_clip);
    const QRectF draw_box(text_clip.left(),
                          text_clip.top(),
                          std::max(text_clip.width(), np::label_draw_width),
                          text_clip.height());
    painter.drawText(draw_box, Qt::AlignVCenter | Qt::AlignLeft, this->category);
    painter.restore();
  }

  // the mark that says "opens a list" belongs to the row being aimed at;
  // on every row at once it is a column of noise
  if (heat <= np::hidden_below)
    return;

  painter.setOpacity(label_alpha * heat);
  QPen pen(this->style.text_active);
  pen.setWidthF(np::chevron_pen);
  pen.setCapStyle(Qt::RoundCap);
  pen.setJoinStyle(Qt::RoundJoin);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  const double  arm = np::chevron_arm;
  const QPointF tip(box.right() - pad - np::chevron_travel * (1.0 - heat), cy);
  painter.drawPolyline(QPolygonF() << QPointF(tip.x() - arm, tip.y() - arm) << tip
                                   << QPointF(tip.x() - arm, tip.y() + arm));
}

void CategoryRailButton::retarget_surface()
{
  const QColor target = this->target_surface();
  const double heat_goal = (this->open || this->hovering) ? 1.0 : 0.0;
  const double open_goal = this->open ? 1.0 : 0.0;

  if (!this->style.animations || this->style.animation_ms <= 0)
  {
    // "off" has to mean off *now*, not after the current fade finishes
    this->surface_animation.stop();
    this->surface = target;
    this->heat_t.set(heat_goal);
    this->open_t.set(open_goal);
    this->update();
    return;
  }

  const bool entering = heat_goal > 0.5;
  this->heat_t.to(heat_goal,
                  entering ? motion::hover_in : motion::hover_out,
                  entering ? motion::enter() : motion::leave());
  this->open_t.to(open_goal,
                  this->open ? motion::popup_in : motion::popup_out,
                  this->open ? motion::enter() : motion::leave());

  // if already animating towards target, let it continue
  if (this->surface_animation.state() == QAbstractAnimation::Running &&
      this->surface_animation.endValue().value<QColor>() == target)
  {
    return;
  }

  // if already at target and not animating, nothing to do
  if (this->surface_animation.state() == QAbstractAnimation::Stopped &&
      this->surface == target)
  {
    return;
  }

  this->surface_animation.stop();
  if (this->surface == target)
  {
    this->update();
    return;
  }

  this->surface_animation.setDuration(this->style.animation_ms);
  this->surface_animation.setEasingCurve(entering ? motion::enter() : motion::leave());
  this->surface_animation.setStartValue(this->surface);
  this->surface_animation.setEndValue(target);
  this->surface_animation.start();
}

void CategoryRailButton::set_open(bool state)
{
  if (this->open == state)
    return;

  this->open = state;
  if (!state)
  {
    const QPoint global_mouse = QCursor::pos();
    this->hovering = this->rect().contains(this->mapFromGlobal(global_mouse));
  }
  this->retarget_surface();
  this->update();
}

void CategoryRailButton::set_reveal(double amount)
{
  this->reveal = std::clamp(amount, 0.0, 1.0);
  this->update();
}

void CategoryRailButton::set_style(const NodePalettePalette &new_style)
{
  this->style = new_style;
  this->surface_animation.stop();
  this->surface = this->target_surface();
  this->heat_t.set((this->open || this->hovering) ? 1.0 : 0.0);
  this->open_t.set(this->open ? 1.0 : 0.0);
  this->updateGeometry();
  this->update();
}

QSize CategoryRailButton::sizeHint() const
{
  const int text_w = QFontMetrics(this->font()).horizontalAdvance(this->category);
  // glyph, name, and room for the chevron at the end of the row
  return QSize(4 * this->style.rail_padding + this->style.icon_size + text_w +
                   np::chevron_hint,
               this->style.button_height);
}

QColor CategoryRailButton::target_surface() const
{
  if (this->open)
    return this->style.surface_selected;
  if (this->hovering)
    return this->style.surface_hover;
  return this->style.surface;
}

} // namespace hesiod
