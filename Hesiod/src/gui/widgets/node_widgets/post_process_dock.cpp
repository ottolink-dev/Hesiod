/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

#include <QAbstractButton>
#include <QContextMenuEvent>
#include <QEnterEvent>
#include <QFocusEvent>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include "meta/core/attribute.hpp"
#include "meta/core/attribute_container.hpp"
#include "meta/core/container_group.hpp"
#include "meta/core/data_provider.hpp"
#include "meta/metadata/keys.hpp"
#include "meta_common.hpp"
#include "meta_qt/designs/industrial/section.hpp"
#include "meta_qt/meta_widget.hpp"
#include "meta_qt/ui/design_registry.hpp"
#include "meta_qt/ui/glide.hpp"
#include "meta_qt/ui/theme.hpp"
#include "meta_qt/widgets/range_bar.hpp"

#include "hesiod/gui/widgets/node_widgets/post_process_dock.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

namespace
{

using meta::qt::Theme;

// --- geometry

constexpr int SCOPE_SIZE = 22;  ///< the display at the head of a stage
constexpr int GUTTER = 32;      ///< the scope and the gap to the stage's rows
constexpr int STAGE_PAD = 2;    ///< air above and below a stage
constexpr int CHIP_SIZE = 24;   ///< a tray toggle
constexpr int CHIP_GAP = 2;     ///< between toggles of one family
constexpr int GROUP_GAP = 11;   ///< between families, a divider sits in it
constexpr int TRAY_HEIGHT = 34; ///< the tray, on one line
constexpr int RANGE_HEIGHT = 52;

/// Dynamic property a stage row sets to say where its first line is centred.
constexpr char HEAD_CENTRE_PROPERTY[] = "stage_head_centre";

QColor with_alpha(QColor color, qreal alpha)
{
  color.setAlphaF(alpha);
  return color;
}

/// `a` at 0, `b` at 1, straight through RGB in between.
QColor mix_colors(const QColor &a, const QColor &b, qreal t)
{
  t = std::clamp(t, 0.0, 1.0);
  return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                          a.greenF() + (b.greenF() - a.greenF()) * t,
                          a.blueF() + (b.blueF() - a.blueF()) * t,
                          a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

// --- glyphs

/** @brief What a glyph is drawn from.
 *
 * One pair covers every effect: the exponent for gamma and gain, the radius as
 * a fraction of its range for smoothing, the two ends for the ranges.
 */
struct GlyphParams
{
  float a = 0.f;
  float b = 0.f;
};

/// The shape an effect is recognised by, used on the tray.
GlyphParams canonical_glyph(PostEffect effect)
{
  switch (effect)
  {
  case PostEffect::GAMMA:
    return {2.4f, 0.f};
  case PostEffect::GAIN:
    return {2.6f, 0.f};
  case PostEffect::SMOOTHING:
    return {0.35f, 0.f};
  case PostEffect::REMAP:
    return {0.06f, 0.94f};
  case PostEffect::SATURATE:
    return {0.3f, 0.7f};
  default:
    return {};
  }
}

/// The same shape, from what the node is actually set to.
GlyphParams model_glyph(const meta::AttributeContainer &container, PostEffect effect)
{
  GlyphParams glyph = canonical_glyph(effect);

  const auto span = [&container, &glyph](const char *key)
  {
    const meta::AbstractAttribute *p_attr = container.find(key);
    const auto *p_range = p_attr ? p_attr->try_cast<meta::Attribute<glm::vec2>>()
                                 : nullptr;
    if (!p_range)
      return;

    const float lo = meta::common::min<float>(*p_range);
    const float hi = meta::common::max<float>(*p_range);
    if (!(hi > lo))
      return;

    glyph.a = (p_range->value().x - lo) / (hi - lo);
    glyph.b = (p_range->value().y - lo) / (hi - lo);
  };

  switch (effect)
  {
  case PostEffect::GAMMA:
    if (const float *p = container.try_value<float>(post_keys::GAMMA))
      glyph.a = *p;
    break;

  case PostEffect::GAIN:
    if (const float *p = container.try_value<float>(post_keys::GAIN))
      glyph.a = *p;
    break;

  case PostEffect::SMOOTHING:
  {
    const meta::AbstractAttribute *p_attr = container.find(post_keys::SMOOTHING_RADIUS);
    const auto *p_radius = p_attr ? p_attr->try_cast<meta::Attribute<float>>() : nullptr;
    if (p_radius)
    {
      const float radius_max = meta::common::max(*p_radius);
      glyph.a = radius_max > 0.f ? p_radius->value() / radius_max : 0.f;
    }
    break;
  }

  case PostEffect::REMAP:
    span(post_keys::REMAP);
    break;
  case PostEffect::SATURATE:
    span(post_keys::SATURATE);
    break;
  default:
    break;
  }

  return glyph;
}

/** @brief Draw an effect as what it does to the signal.
 *
 * Gamma, gain and saturate are their own transfer curves, input left to right
 * and output bottom to top, so the glyph on a stage bends as its slider moves.
 * Smoothing is its kernel, remap the two levels it stretches between. Invert
 * and mix have no curve worth the name and keep a pictogram.
 */
void paint_glyph(QPainter          &painter,
                 const QRectF      &box,
                 PostEffect         effect,
                 const GlyphParams &glyph,
                 const QColor      &ink)
{
  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setPen(QPen(ink, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.setBrush(Qt::NoBrush);

  // the unit square, y up
  const auto at = [&box](qreal x, qreal y)
  { return QPointF(box.left() + x * box.width(), box.bottom() - y * box.height()); };

  const auto trace = [&painter, &at](const std::function<qreal(qreal)> &f)
  {
    constexpr int steps = 24;
    QPainterPath  path;

    for (int i = 0; i <= steps; ++i)
    {
      const qreal   x = qreal(i) / steps;
      const QPointF point = at(x, std::clamp(f(x), 0.0, 1.0));
      if (i == 0)
        path.moveTo(point);
      else
        path.lineTo(point);
    }

    painter.drawPath(path);
  };

  switch (effect)
  {
  case PostEffect::MIX:
  {
    const qreal   r = 0.32 * box.width();
    const QPointF left(box.center().x() - 0.56 * r, box.center().y());
    const QPointF right(box.center().x() + 0.56 * r, box.center().y());

    QPainterPath a;
    QPainterPath b;
    a.addEllipse(left, r, r);
    b.addEllipse(right, r, r);

    painter.fillPath(a.intersected(b), with_alpha(ink, 0.5 * ink.alphaF()));
    painter.drawPath(a);
    painter.drawPath(b);
    break;
  }

  case PostEffect::INVERT:
  {
    const qreal  r = 0.42 * box.width();
    const QRectF disc(box.center().x() - r, box.center().y() - r, 2 * r, 2 * r);

    QPainterPath half;
    half.moveTo(disc.center().x(), disc.top());
    half.arcTo(disc, 90, -180);
    half.closeSubpath();

    painter.fillPath(half, ink);
    painter.drawEllipse(disc);
    break;
  }

  case PostEffect::GAMMA:
  {
    const qreal gamma = std::max(0.01f, glyph.a);
    trace([gamma](qreal x) { return std::pow(x, gamma); });
    break;
  }

  case PostEffect::GAIN:
  {
    // hmap::gain
    const qreal gain = std::max(0.01f, glyph.a);
    trace(
        [gain](qreal x)
        {
          return x < 0.5 ? 0.5 * std::pow(2.0 * x, gain)
                         : 1.0 - 0.5 * std::pow(2.0 * (1.0 - x), gain);
        });
    break;
  }

  case PostEffect::SMOOTHING:
  {
    // narrow and tall with no radius to speak of, low and wide with a lot
    const qreal t = std::clamp(qreal(glyph.a), 0.0, 1.0);
    const qreal sigma = 0.09 + 0.26 * t;
    const qreal peak = 0.94 - 0.42 * t;
    trace(
        [sigma, peak](qreal x)
        {
          const qreal d = (x - 0.5) / sigma;
          return 0.06 + (peak - 0.06) * std::exp(-d * d);
        });
    break;
  }

  case PostEffect::REMAP:
  {
    qreal lo = std::clamp(qreal(glyph.a), 0.0, 1.0);
    qreal hi = std::clamp(qreal(glyph.b), 0.0, 1.0);

    // two levels have to stay two levels however narrow the range gets
    if (hi - lo < 0.3)
    {
      const qreal mid = std::clamp(0.5 * (lo + hi), 0.15, 0.85);
      lo = mid - 0.15;
      hi = mid + 0.15;
    }

    painter.drawLine(at(0.08, hi), at(0.92, hi));
    painter.drawLine(at(0.08, lo), at(0.92, lo));

    if (hi - lo > 0.5)
    {
      const qreal head = 0.16;
      painter.drawLine(at(0.5, lo + 0.16), at(0.5, hi - 0.16));
      painter.drawLine(at(0.5, hi - 0.16), at(0.5 - head, hi - 0.16 - head));
      painter.drawLine(at(0.5, hi - 0.16), at(0.5 + head, hi - 0.16 - head));
      painter.drawLine(at(0.5, lo + 0.16), at(0.5 - head, lo + 0.16 + head));
      painter.drawLine(at(0.5, lo + 0.16), at(0.5 + head, lo + 0.16 + head));
    }
    break;
  }

  case PostEffect::SATURATE:
  {
    const qreal lo = std::clamp(qreal(glyph.a), 0.0, 0.9);
    const qreal hi = std::clamp(qreal(glyph.b), lo + 0.1, 1.0);
    trace([lo, hi](qreal x)
          { return 0.12 + 0.76 * std::clamp((x - lo) / (hi - lo), 0.0, 1.0); });
    break;
  }
  }

  painter.restore();
}

void style_menu(QMenu &menu, const Theme &theme)
{
  menu.setWindowFlag(Qt::NoDropShadowWindowHint);
  menu.setFont(meta::qt::ui_font(12));
  menu.setStyleSheet(
      QString("QMenu { background: %1; color: %2; border: 1px solid %3;"
              " border-radius: 8px; padding: 6px; }"
              "QMenu::item { padding: 7px 18px 7px 10px; border-radius: 4px; }"
              "QMenu::item:selected { background: %4; }"
              "QMenu::separator { height: 1px; background: %3; margin: 5px 8px; }")
          .arg(theme.bar.name(),
               theme.ink_primary.name(),
               theme.field_border.name(),
               theme.section_header_hover.name()));
}

// =====================================
// EffectChip
// =====================================

/// One effect on the tray: lit when it is in the chain, a click moves it in or out.
class EffectChip final : public QAbstractButton
{
public:
  EffectChip(PostEffect effect, const Theme &theme, QWidget *parent)
      : QAbstractButton(parent), effect(effect), theme(theme)
  {
    const PostEffectInfo &info = post_effect_info(effect);

    this->setCheckable(true);
    this->setFixedSize(CHIP_SIZE, CHIP_SIZE);
    this->setCursor(Qt::PointingHandCursor);
    this->setAttribute(Qt::WA_Hover);
    this->setFocusPolicy(Qt::TabFocus);
    this->setAccessibleName(QString::fromStdString(info.name));
    this->setToolTip(QString::fromStdString(info.name + ": " + info.hint));

    this->lit = new meta::qt::Glide(theme.metrics.switch_ms, this);
    this->hover = new meta::qt::Glide(theme.metrics.switch_ms, this);

    for (meta::qt::Glide *glide : {this->lit, this->hover})
      QObject::connect(glide, &meta::qt::Glide::tick, this, [this]() { this->update(); });
  }

  const PostEffect          effect;
  std::function<void(bool)> on_hover;

protected:
  void enterEvent(QEnterEvent *event) override
  {
    // in at once, out over a moment: a pointer crossing the tray lights each
    // chip as it reaches it and leaves a short trail behind
    this->hover->jump(1.0);

    if (this->on_hover)
      this->on_hover(true);
    QAbstractButton::enterEvent(event);
  }

  void leaveEvent(QEvent *event) override
  {
    this->hover->to(0.0);

    if (this->on_hover)
      this->on_hover(false);
    QAbstractButton::leaveEvent(event);
  }

  void paintEvent(QPaintEvent *) override
  {
    // Followed from here rather than from toggled(): the dock sets the check
    // state with signals blocked when it syncs from the model, and the chip
    // has to glide for an undo just as it does for a click. The first paint
    // seats the state, there is nothing to animate from yet.
    if (!this->seated)
    {
      this->seated = true;
      this->lit_target = this->isChecked();
      const QSignalBlocker blocker(this->lit);
      this->lit->jump(this->lit_target ? 1.0 : 0.0);
    }
    else if (this->lit_target != this->isChecked())
    {
      this->lit_target = this->isChecked();
      this->lit->to(this->lit_target ? 1.0 : 0.0);
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal  on = this->lit->current();
    const qreal  over = this->hover->current();
    const bool   down = this->isDown();
    const QRectF frame = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    // off: nothing until the pointer arrives
    if (over > 0.0 || down)
    {
      painter.setPen(Qt::NoPen);
      painter.setBrush(with_alpha(down ? this->theme.section_header_press
                                       : this->theme.section_header_hover,
                                  (down ? 1.0 : over) * (1.0 - on)));
      painter.drawRoundedRect(frame, 6, 6);
    }

    // on: the accent, as a tint and an outline
    if (on > 0.0)
    {
      painter.setPen(QPen(with_alpha(this->theme.accent, on * (0.7 + 0.25 * over)), 1));
      painter.setBrush(
          with_alpha(this->theme.accent, on * (down ? 0.38 : 0.22 + 0.08 * over)));
      painter.drawRoundedRect(frame, 6, 6);
    }

    if (this->hasFocus())
    {
      painter.setPen(QPen(this->theme.accent, 1));
      painter.setBrush(Qt::NoBrush);
      painter.drawRoundedRect(frame, 6, 6);
    }

    paint_glyph(
        painter,
        QRectF(this->rect()).adjusted(5.5, 5.5, -5.5, -5.5),
        this->effect,
        canonical_glyph(this->effect),
        mix_colors(this->theme.ink_dim, this->theme.ink_primary, std::max(on, over)));
  }

private:
  const Theme     &theme;
  meta::qt::Glide *lit = nullptr;   ///< 0 off, 1 in the chain
  meta::qt::Glide *hover = nullptr; ///< 0 away, 1 under the pointer
  bool             lit_target = false;
  bool             seated = false;
};

// =====================================
// StageScope
// =====================================

/** @brief The display at the head of a stage.
 *
 * Shows what the stage is currently doing to the signal, and doubles as its
 * remove button: under the pointer the trace gives way to a cross. A separate
 * button for that would cost the slider next to it a fifth of its rail.
 *
 * Drawn as a value field is, in the same grays: the surface and outline of the
 * number at the other end of the row, with the icon ink for the trace. A dark
 * well with an accent trace made a column of these the heaviest thing in the
 * panel.
 */
class StageScope final : public QAbstractButton
{
public:
  StageScope(PostEffect effect, const Theme &theme, QWidget *parent)
      : QAbstractButton(parent), effect(effect), theme(theme),
        glyph(canonical_glyph(effect))
  {
    const QString name = QString::fromStdString(post_effect_info(effect).name);

    this->setFixedSize(SCOPE_SIZE, SCOPE_SIZE);
    this->setCursor(Qt::PointingHandCursor);
    this->setAttribute(Qt::WA_Hover);
    this->setFocusPolicy(Qt::TabFocus);
    this->setAccessibleName(QObject::tr("Remove %1").arg(name));
    this->setToolTip(QObject::tr("Remove %1").arg(name));

    this->armed = new meta::qt::Glide(theme.metrics.switch_ms, this);
    QObject::connect(this->armed,
                     &meta::qt::Glide::tick,
                     this,
                     [this]() { this->update(); });
  }

  void set_glyph(const GlyphParams &new_glyph)
  {
    if (new_glyph.a == this->glyph.a && new_glyph.b == this->glyph.b)
      return;

    this->glyph = new_glyph;
    this->update();
  }

protected:
  void enterEvent(QEnterEvent *event) override
  {
    this->armed->to(1.0);
    QAbstractButton::enterEvent(event);
  }

  void leaveEvent(QEvent *event) override
  {
    this->armed->to(this->hasFocus() ? 1.0 : 0.0);
    QAbstractButton::leaveEvent(event);
  }

  void focusInEvent(QFocusEvent *event) override
  {
    this->armed->to(1.0);
    QAbstractButton::focusInEvent(event);
  }

  void focusOutEvent(QFocusEvent *event) override
  {
    this->armed->to(this->underMouse() ? 1.0 : 0.0);
    QAbstractButton::focusOutEvent(event);
  }

  void paintEvent(QPaintEvent *) override
  {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal  t = this->armed->current();
    const QRectF well = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    painter.setPen(
        QPen(mix_colors(this->theme.field_border, this->theme.field_border_hover, t), 1));
    painter.setBrush(this->isDown() ? this->theme.field_editing : this->theme.field);
    painter.drawRoundedRect(well, 6, 6);

    // the trace and the cross trade places rather than swap
    if (t < 1.0)
    {
      painter.setOpacity(1.0 - t);
      paint_glyph(painter,
                  QRectF(this->rect()).adjusted(5, 5, -5, -5),
                  this->effect,
                  this->glyph,
                  this->theme.ink_icon);
    }

    if (t > 0.0)
    {
      const QPointF c = well.center();
      const qreal   d = 2.0 + 1.5 * t;

      painter.setOpacity(t);
      painter.setPen(QPen(this->theme.ink_primary, 1.5, Qt::SolidLine, Qt::RoundCap));
      painter.drawLine(c + QPointF(-d, -d), c + QPointF(d, d));
      painter.drawLine(c + QPointF(-d, d), c + QPointF(d, -d));
    }
  }

private:
  const PostEffect effect;
  const Theme     &theme;
  GlyphParams      glyph;
  meta::qt::Glide *armed = nullptr; ///< 0 showing the trace, 1 the cross
};

// =====================================
// StageLabel
// =====================================

/// A stage with nothing to set (invert) still gets a line saying it is there.
class StageLabel final : public QWidget
{
public:
  StageLabel(const QString &text, const Theme &theme, QWidget *parent)
      : QWidget(parent), text(text), theme(theme)
  {
    this->setFixedHeight(theme.metrics.row_height);
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  }

protected:
  void paintEvent(QPaintEvent *) override
  {
    QPainter painter(this);
    painter.setFont(meta::qt::row_label_font());
    // on at all means off its default, which is what the brighter ink is for
    painter.setPen(this->theme.state_ink(true, false));
    painter.drawText(
        this->rect(),
        Qt::AlignLeft | Qt::AlignVCenter,
        painter.fontMetrics().elidedText(this->text, Qt::ElideRight, this->width()));
  }

private:
  QString      text;
  const Theme &theme;
};

// =====================================
// RangeEditor
// =====================================

/** @brief Label and range bar on one line, for the two range stages.
 *
 * The stock range row stacks a label, the bar and a row of buttons, one of
 * which switches the range on and off. Here the stage being in the chain is
 * the switch, the label takes the same column a slider's does, and the three
 * presets live in the stage's context menu, which leaves a row about the
 * height of the bar.
 */
class RangeEditor final : public QWidget
{
public:
  RangeEditor(meta::Attribute<glm::vec2> &attr,
              const meta::qt::RowContext &ctx,
              const Theme                &theme,
              QWidget                    *parent)
      : QWidget(parent), attr(attr), ctx(ctx), theme(theme),
        label(QString::fromStdString(meta::common::label(attr))),
        vmin(meta::common::min<float>(attr)), vmax(meta::common::max<float>(attr))
  {
    this->setFixedHeight(RANGE_HEIGHT);
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    const int decimals = meta::common::try_get_format_decimals(
        meta::common::format(attr));

    this->bar = new meta::qt::RangeBar(attr.value(),
                                       this->vmin,
                                       this->vmax,
                                       decimals,
                                       this);
    this->bar->set_theme(theme);
    this->bar->setFixedHeight(RANGE_HEIGHT);

    // the bar reads its readouts and histogram colours from the palette
    QPalette palette = this->bar->palette();
    palette.setColor(QPalette::Text, theme.ink_secondary);
    palette.setColor(QPalette::Dark, theme.rail_well_border);
    palette.setColor(QPalette::Highlight, theme.accent);
    this->bar->setPalette(palette);

    if (const auto *p_meta = attr.metadata().find(meta::keys::ui::data_provider))
      if (const auto *p_provider = p_meta
                                       ->try_cast<meta::Attribute<meta::DataProvider>>())
        this->provider = p_provider->value();

    this->connect(this->bar,
                  &meta::qt::RangeBar::value_changed,
                  this,
                  [this](glm::vec2 range)
                  {
                    if (!this->editing)
                    {
                      this->editing = true;
                      if (this->on_edit_started)
                        this->on_edit_started();
                    }

                    this->remember(range);
                    this->attr.set_from_any(range);
                    this->refresh_modified();

                    if (this->on_value_changed)
                      this->on_value_changed();
                  });

    this->connect(this->bar,
                  &meta::qt::RangeBar::drag_ended,
                  this,
                  [this](glm::vec2 range)
                  {
                    this->attr.set_from_any(range);
                    this->editing = false;
                    this->refresh_modified();

                    if (this->on_edit_ended)
                      this->on_edit_ended();
                  });

    this->sync();
  }

  std::function<void()> on_edit_started;
  std::function<void()> on_value_changed;
  std::function<void()> on_edit_ended;

  float     domain_min() const { return this->vmin; }
  float     domain_max() const { return this->vmax; }
  glm::vec2 value() const { return this->attr.value(); }

  /// Seat a range as one complete edit, for the presets.
  void commit(glm::vec2 range)
  {
    range.x = std::clamp(range.x, this->vmin, this->vmax);
    range.y = std::clamp(range.y, this->vmin, this->vmax);
    if (range.x > range.y)
      std::swap(range.x, range.y);

    if (this->on_edit_started)
      this->on_edit_started();

    this->remember(range);
    this->attr.set_from_any(range);
    this->bar->set_value(range);
    this->refresh_modified();

    if (this->on_value_changed)
      this->on_value_changed();
    if (this->on_edit_ended)
      this->on_edit_ended();
  }

  /// Model to widget.
  void sync()
  {
    if (this->editing)
      return;

    {
      const QSignalBlocker blocker(this->bar);
      this->bar->set_value(this->attr.value());
    }

    this->refresh_modified();

    if (!this->provider)
      return;

    try
    {
      meta::Any payload = this->provider();
      if (auto *p_histogram = payload.get<meta::qt::HistogramData>())
        this->bar->set_histogram(p_histogram->x, p_histogram->y);
      else
        this->bar->set_histogram({}, {});
    }
    catch (...)
    {
      // a provider reading a port that is being recomputed; keep the last one
    }
  }

  QSize sizeHint() const override
  {
    return QSize(this->theme.metrics.label_min_width + 200, this->height());
  }

protected:
  void resizeEvent(QResizeEvent *event) override
  {
    // The bar pads its track by 10 px on both sides for the handles. Start it
    // that much early, so the track begins where a slider's rail does, and let
    // half the padding run off the far end, so the upper handle comes to rest
    // against the edge the value fields end on.
    const int beside = this->label_width() + this->theme.metrics.gap - 10;

    // A slider that runs out of room drops its rail and keeps the number. A
    // range has nothing but its rail, so on a narrow panel it moves under the
    // label and takes the whole width instead.
    this->stacked = this->width() - beside < MIN_BAR_WIDTH;

    if (this->stacked)
      this->bar->setGeometry(-5, LABEL_LINE, this->width() + 10, RANGE_HEIGHT);
    else
      this->bar->setGeometry(beside, 0, this->width() - beside + 5, RANGE_HEIGHT);

    // where the stage's scope should sit: level with the label
    const int label_centre = this->stacked ? LABEL_LINE / 2 : (RANGE_HEIGHT - 8) / 2;
    if (this->property(HEAD_CENTRE_PROPERTY) != QVariant(label_centre))
    {
      this->setProperty(HEAD_CENTRE_PROPERTY, label_centre);
      this->updateGeometry(); // has the stage lay out again, and read it
    }

    this->setFixedHeight(this->stacked ? LABEL_LINE + RANGE_HEIGHT : RANGE_HEIGHT);
    QWidget::resizeEvent(event);
  }

  void paintEvent(QPaintEvent *) override
  {
    QPainter painter(this);
    painter.setFont(meta::qt::row_label_font());
    painter.setPen(this->theme.state_ink(this->modified, false));

    // beside the bar, centred on its track, which it draws above its middle
    const QRect cell = this->stacked ? QRect(0, 0, this->width(), LABEL_LINE)
                                     : QRect(0, 0, this->label_width(), RANGE_HEIGHT - 8);

    painter.drawText(
        cell,
        Qt::AlignLeft | Qt::AlignVCenter,
        painter.fontMetrics().elidedText(this->label, Qt::ElideRight, cell.width()));
  }

private:
  static constexpr int MIN_BAR_WIDTH = 110; ///< narrower, and the bar goes under
  static constexpr int LABEL_LINE = 24;     ///< the label's own line, when it does

  /// The column a slider row gives its label, so the two line up.
  int label_width() const
  {
    const meta::qt::Metrics &m = this->theme.metrics;
    return int(std::clamp<qreal>(this->width() * m.label_width_ratio,
                                 m.label_min_width,
                                 m.label_max_width));
  }

  void remember(const glm::vec2 &range)
  {
    meta::AttributeContainer &state = this->attr.state();
    if (auto *p_last = state.try_value<glm::vec2>(meta::keys::state::last_active_value))
      *p_last = range;
    else
      state.try_add(meta::keys::state::last_active_value, glm::vec2(range));
  }

  void refresh_modified()
  {
    bool now_modified = false;

    if (this->ctx.default_value)
    {
      const std::any initial = this->ctx.default_value(this->attr.name());
      if (const auto *p_initial = std::any_cast<glm::vec2>(&initial))
        now_modified = glm::distance(*p_initial, this->attr.value()) > 1e-4f;
    }

    if (now_modified != this->modified)
    {
      this->modified = now_modified;
      this->update();
    }
  }

  meta::Attribute<glm::vec2> &attr;
  meta::qt::RowContext        ctx;
  const Theme                &theme;
  QString                     label;
  float                       vmin;
  float                       vmax;
  meta::qt::RangeBar         *bar = nullptr;
  meta::DataProvider          provider;
  bool                        editing = false;
  bool                        modified = false;
  bool                        stacked = false;
};

// =====================================
// StageRow
// =====================================

/// One effect in the chain: its scope in the gutter, its rows beside it.
class StageRow final : public QWidget
{
public:
  StageRow(PostEffect effect, const Theme &theme, QWidget *parent)
      : QWidget(parent), theme(theme)
  {
    this->content = new QVBoxLayout(this);
    this->content->setContentsMargins(GUTTER, STAGE_PAD, 0, STAGE_PAD);
    this->content->setSpacing(2);

    this->scope = new StageScope(effect, theme, this);
    this->place_scope();
  }

  /// The first stage has nothing above it to wire to.
  void set_first(bool new_first)
  {
    if (new_first == this->first)
      return;

    this->first = new_first;
    this->update();
  }

  QVBoxLayout                 *content = nullptr;
  StageScope                  *scope = nullptr;
  std::function<void(QMenu &)> fill_menu;

protected:
  bool event(QEvent *event) override
  {
    const bool handled = QWidget::event(event);

    // rows arrive after the stage is built, and a range changes shape with
    // the panel's width
    if (event->type() == QEvent::LayoutRequest)
      this->place_scope();

    return handled;
  }

  void paintEvent(QPaintEvent *) override
  {
    QPainter painter(this);
    painter.setPen(QPen(with_alpha(this->theme.ink_dim, 0.45), 1));

    const qreal x = SCOPE_SIZE / 2 + 0.5;
    const QRect head = this->scope->geometry();

    // down to the next stage, or to the terminal on the tray
    if (!this->first)
      painter.drawLine(QPointF(x, 0), QPointF(x, head.top() - 3));
    painter.drawLine(QPointF(x, head.bottom() + 4), QPointF(x, this->height()));
  }

  void contextMenuEvent(QContextMenuEvent *event) override
  {
    if (!this->fill_menu)
      return;

    QMenu menu(this);
    style_menu(menu, this->theme);
    this->fill_menu(menu);

    if (!menu.isEmpty())
      menu.exec(event->globalPos());
  }

private:
  /** @brief Put the scope level with the stage's first line.
   *
   * How tall that line is belongs to the design the rows are built with: a
   * slider is a full row in one and barely taller than its text in another. A
   * row whose first line is not simply its upper half says where it is.
   */
  void place_scope()
  {
    int centre = this->theme.metrics.row_height / 2;

    if (QLayoutItem *item = this->content->itemAt(0))
      if (QWidget *head = item->widget())
      {
        const QVariant declared = head->property(HEAD_CENTRE_PROPERTY);
        centre = declared.isValid() ? declared.toInt()
                                    : std::min(head->sizeHint().height(),
                                               this->theme.metrics.row_height) /
                                          2;
      }

    const QPoint spot(0, STAGE_PAD + std::max(0, centre - SCOPE_SIZE / 2));
    if (spot != this->scope->pos())
    {
      this->scope->move(spot);
      this->update();
    }
  }

  const Theme &theme;
  bool         first = false;
};

// =====================================
// Tray
// =====================================

int chip_family(PostEffect effect)
{
  switch (effect)
  {
  case PostEffect::MIX:
    return 0;
  case PostEffect::INVERT:
  case PostEffect::GAMMA:
  case PostEffect::GAIN:
    return 1;
  case PostEffect::SMOOTHING:
    return 2;
  default:
    return 3;
  }
}

/** @brief Every effect the node offers, as a row of toggles.
 *
 * The one part of the dock that is always there, and its last row: the stages
 * stack upwards from it. Attached to the foot of the properties panel, which
 * is where a node shown on its own has it, that makes the tray the part that
 * never moves. A toggle that has just been clicked is still under the pointer,
 * ready to be clicked again, and the dock grows away from it.
 *
 * The chain ends here. The terminal on the left is where the wire through the
 * stages arrives, the node's output, and it is hollow while the chain is empty.
 */
class Tray final : public QWidget
{
public:
  Tray(const Theme &theme, QWidget *parent) : QWidget(parent), theme(theme)
  {
    this->setFixedHeight(TRAY_HEIGHT);
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    this->lit = new meta::qt::Glide(theme.metrics.section_ms, this);
    QObject::connect(this->lit,
                     &meta::qt::Glide::tick,
                     this,
                     [this]() { this->update(); });
  }

  void clear()
  {
    for (EffectChip *chip : this->chips)
    {
      chip->hide();
      chip->deleteLater();
    }

    this->chips.clear();
    this->hint.clear();
    this->update();
  }

  EffectChip *add_chip(PostEffect effect)
  {
    auto *chip = new EffectChip(effect, this->theme, this);
    chip->on_hover = [this, chip](bool inside)
    {
      const QString name = QString::fromStdString(post_effect_info(chip->effect).name);

      if (inside)
        this->hint = chip->isChecked() ? QObject::tr("Remove %1").arg(name)
                                       : QObject::tr("Add %1").arg(name);
      else
        this->hint.clear();

      this->update();
    };

    chip->show();
    this->chips.push_back(chip);
    this->place_chips();
    return chip;
  }

  /// Whether anything is in the chain, which is what lights the terminal.
  void set_live(bool new_live)
  {
    if (new_live == this->live)
      return;

    this->live = new_live;

    // nothing to animate from on a tray that has not been on screen yet
    if (this->isVisible())
      this->lit->to(new_live ? 1.0 : 0.0);
    else
      this->lit->jump(new_live ? 1.0 : 0.0);
  }

  /// The hint reads "Add" or "Remove", so it goes stale when a chip flips.
  void refresh_hint()
  {
    for (EffectChip *chip : this->chips)
      if (chip->underMouse() && chip->on_hover)
        chip->on_hover(true);
  }

protected:
  void resizeEvent(QResizeEvent *event) override
  {
    this->place_chips();
    QWidget::resizeEvent(event);
  }

  void paintEvent(QPaintEvent *) override
  {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // --- terminal, and the wire reaching it once there is a stage above. The
    // ring fills in as the first stage arrives rather than switching.
    if (this->origin >= GUTTER)
    {
      const qreal   t = this->lit->current();
      const QPointF centre(SCOPE_SIZE / 2 + 0.5, this->height() / 2 + 0.5);

      if (t > 0.0)
      {
        painter.setPen(QPen(with_alpha(this->theme.ink_dim, 0.45 * t), 1));
        painter.drawLine(QPointF(centre.x(), 0), centre - QPointF(0, 6));

        painter.setPen(Qt::NoPen);
        painter.setBrush(with_alpha(this->theme.accent, t));
        painter.drawEllipse(centre, 2.0 + 1.5 * t, 2.0 + 1.5 * t);
      }

      if (t < 1.0)
      {
        painter.setPen(QPen(with_alpha(this->theme.ink_dim, 1.0 - t), 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(centre, 3.25, 3.25);
      }
    }

    // --- dividers between the chip families
    painter.setPen(QPen(with_alpha(this->theme.ink_dim, 0.3), 1));
    for (int x : this->dividers)
      painter.drawLine(QPointF(x + 0.5, this->height() / 2 - 6),
                       QPointF(x + 0.5, this->height() / 2 + 6));

    // --- caption, or what the chip under the pointer would do
    const bool    hinting = !this->hint.isEmpty();
    const QString text = hinting ? this->hint : QObject::tr("Post-process");
    const QRect   cell(this->chips_end + 10,
                     0,
                     this->width() - this->chips_end - 10,
                     this->height());

    painter.setFont(meta::qt::ui_font(12));
    if (painter.fontMetrics().horizontalAdvance(text) > cell.width())
      return; // the tooltips carry it on a panel this narrow

    painter.setPen(hinting ? this->theme.ink_secondary : this->theme.ink_dim);
    painter.drawText(cell, Qt::AlignRight | Qt::AlignVCenter, text);
  }

private:
  void place_chips()
  {
    this->dividers.clear();

    const auto total = [this](int gap, int group_gap)
    {
      int width = 0;
      for (size_t i = 0; i < this->chips.size(); ++i)
      {
        if (i > 0)
          width += chip_family(this->chips[i]->effect) ==
                           chip_family(this->chips[i - 1]->effect)
                       ? gap
                       : group_gap;
        width += CHIP_SIZE;
      }
      return width;
    };

    // Give ground in steps on a narrow panel: first the air between families,
    // then the terminal's gutter.
    int gap = CHIP_GAP;
    int group_gap = GROUP_GAP;
    this->origin = GUTTER;

    if (this->origin + total(gap, group_gap) > this->width())
      group_gap = CHIP_GAP + 2;
    if (this->origin + total(gap, group_gap) > this->width())
      this->origin = 0;

    int       x = this->origin;
    const int y = (this->height() - CHIP_SIZE) / 2;

    for (size_t i = 0; i < this->chips.size(); ++i)
    {
      if (i > 0)
      {
        const bool same = chip_family(this->chips[i]->effect) ==
                          chip_family(this->chips[i - 1]->effect);
        if (!same && group_gap == GROUP_GAP)
          this->dividers.push_back(x + group_gap / 2);
        x += same ? gap : group_gap;
      }

      this->chips[i]->move(x, y);
      x += CHIP_SIZE;
    }

    this->chips_end = x;
    this->update();
  }

  const Theme              &theme;
  std::vector<EffectChip *> chips;
  std::vector<int>          dividers;
  QString                   hint;
  int                       origin = GUTTER;
  int                       chips_end = GUTTER;
  bool                      live = false;
  meta::qt::Glide          *lit = nullptr; ///< 0 chain empty, 1 something in it
};

} // namespace

// =====================================
// PostProcessDock
// =====================================

struct PostProcessDock::Impl
{
  struct Stage
  {
    PostEffect                          effect;
    meta::qt::industrial::ClipBox      *clip = nullptr;
    StageRow                           *row = nullptr;
    EffectChip                         *chip = nullptr;
    QVariantAnimation                  *reveal = nullptr;
    RangeEditor                        *range = nullptr;
    std::vector<meta::qt::MetaWidget *> rows;
    bool                                shown = false;
  };

  PostProcessDock     *q = nullptr;
  NodeResolver         resolver;
  std::string          design;
  meta::qt::RowContext ctx;
  const Theme         *theme = nullptr;
  bool                 as_card = false;

  Tray        *tray = nullptr;
  QVBoxLayout *stage_layout = nullptr;

  /// Container the stages are bound to; a node switching to another rebuilds.
  std::string        container_name;
  std::vector<Stage> stages;

  /// The stage whose own row is being dragged, see sync().
  std::optional<PostEffect> editing;

  meta::AttributeContainer *container() const
  {
    BaseNode *p_node = this->resolver ? this->resolver() : nullptr;
    return p_node ? &p_node->get_meta_group().current() : nullptr;
  }

  Stage *find(PostEffect effect)
  {
    for (Stage &stage : this->stages)
      if (stage.effect == effect)
        return &stage;

    return nullptr;
  }

  void rebuild();
  void sync();
  void add_stage(meta::AttributeContainer &container, PostEffect effect);
  void add_row(Stage &stage, meta::AttributeContainer &container, const char *key);
  void add_range(Stage &stage, meta::AttributeContainer &container, const char *key);
  void set_effect(PostEffect effect, bool active);
  void set_shown(Stage &stage, bool shown, bool animate);
  void refresh_scope(PostEffect effect);
  void refresh_chain();

  void begin_stage_edit(PostEffect effect);
  void end_stage_edit(PostEffect effect);
};

void PostProcessDock::Impl::rebuild()
{
  for (Stage &stage : this->stages)
  {
    stage.reveal->stop();
    stage.clip->hide();
    stage.clip->deleteLater();
  }

  this->stages.clear();
  this->tray->clear();
  this->editing.reset();
  this->container_name.clear();

  BaseNode *p_node = this->resolver ? this->resolver() : nullptr;
  if (!p_node)
  {
    this->q->hide();
    return;
  }

  meta::ContainerGroup &group = p_node->get_meta_group();
  this->container_name = group.current_container_name().value_or(std::string{});

  meta::AttributeContainer &container = group.current();

  for (const PostEffectInfo &info : post_effect_catalog())
    if (has_post_effect(container, info.effect))
      this->add_stage(container, info.effect);

  // a node can offer the chain in one container and not in another
  this->q->setVisible(!this->stages.empty());
  this->refresh_chain();
}

void PostProcessDock::Impl::add_stage(meta::AttributeContainer &container,
                                      PostEffect                effect)
{
  Stage stage;
  stage.effect = effect;
  stage.row = new StageRow(effect, *this->theme, nullptr);

  switch (effect)
  {
  case PostEffect::MIX:
    this->add_row(stage, container, post_keys::MIX_METHOD);
    this->add_row(stage, container, post_keys::MIX);
    break;

  case PostEffect::INVERT:
  {
    std::string label = post_effect_info(effect).name;
    if (const auto *p_attr = container.find(post_keys::INVERSE))
      label = meta::common::try_get<std::string>(*p_attr, meta::keys::ui::label, label);

    stage.row->content->addWidget(
        new StageLabel(QString::fromStdString(label), *this->theme, stage.row));
    break;
  }

  case PostEffect::GAMMA:
    this->add_row(stage, container, post_keys::GAMMA);
    break;
  case PostEffect::GAIN:
    this->add_row(stage, container, post_keys::GAIN);
    break;

  case PostEffect::SMOOTHING:
    this->add_row(stage, container, post_keys::SMOOTHING_RADIUS);
    break;

  case PostEffect::REMAP:
    this->add_range(stage, container, post_keys::REMAP);
    break;
  case PostEffect::SATURATE:
    this->add_range(stage, container, post_keys::SATURATE);
    break;
  }

  // --- the reveal. Cropped rather than squeezed, as the sections do it: the
  // stage keeps its full height throughout and only the window onto it moves.
  stage.clip = new meta::qt::industrial::ClipBox(this->q);
  stage.clip->set_body(stage.row);
  this->stage_layout->addWidget(stage.clip);

  stage.reveal = new QVariantAnimation(stage.clip);
  stage.reveal->setDuration(this->theme->metrics.section_ms);
  stage.reveal->setEasingCurve(QEasingCurve::OutCubic);

  auto *clip = stage.clip;
  QObject::connect(stage.reveal,
                   &QVariantAnimation::valueChanged,
                   clip,
                   [clip](const QVariant &value) { clip->set_reveal(value.toInt()); });

  QObject::connect(stage.reveal,
                   &QVariantAnimation::finished,
                   clip,
                   [this, effect]()
                   {
                     Stage *p_stage = this->find(effect);
                     if (!p_stage)
                       return;

                     if (!p_stage->shown)
                     {
                       p_stage->clip->hide();
                       return;
                     }

                     // track the body from here on, a range bar's height is
                     // its own business
                     p_stage->clip->follow_body();

                     // in the flow of a list the dock comes last, and a stage
                     // can open past the edge of what is on screen
                     for (QWidget *w = p_stage->clip->parentWidget(); w;
                          w = w->parentWidget())
                       if (auto *area = qobject_cast<QScrollArea *>(w))
                       {
                         area->ensureWidgetVisible(p_stage->clip, 0, 12);
                         break;
                       }
                   });

  // --- the two ways in and out: the tray toggle, and the stage's own scope
  stage.chip = this->tray->add_chip(effect);

  QObject::connect(stage.chip,
                   &QAbstractButton::clicked,
                   this->q,
                   [this, effect](bool checked) { this->set_effect(effect, checked); });

  QObject::connect(stage.row->scope,
                   &QAbstractButton::clicked,
                   this->q,
                   [this, effect]() { this->set_effect(effect, false); });

  RangeEditor *range = stage.range;
  stage.row->fill_menu = [this, effect, range](QMenu &menu)
  {
    if (range)
    {
      const float lo = range->domain_min();
      const float hi = range->domain_max();

      menu.addAction(QObject::tr("Unit Range (0 to 1)"),
                     range,
                     [range]() { range->commit({0.f, 1.f}); });
      menu.addAction(QObject::tr("Full Range (%1 to %2)").arg(lo).arg(hi),
                     range,
                     [range, lo, hi]() { range->commit({lo, hi}); });
      menu.addAction(QObject::tr("Centre on Zero"),
                     range,
                     [range]()
                     {
                       // same span, moved so that it straddles zero
                       const glm::vec2 current = range->value();
                       const float     half = 0.5f * (current.y - current.x);
                       range->commit({-half, half});
                     });
      menu.addSeparator();
    }

    const QString name = QString::fromStdString(post_effect_info(effect).name);
    menu.addAction(QObject::tr("Remove %1").arg(name),
                   this->q,
                   [this, effect]() { this->set_effect(effect, false); });
  };

  const bool active = is_post_effect_active(container, effect);
  this->stages.push_back(std::move(stage));
  this->set_shown(this->stages.back(), active, /* animate */ false);
  this->refresh_scope(effect);
}

void PostProcessDock::Impl::add_row(Stage                    &stage,
                                    meta::AttributeContainer &container,
                                    const char               *key)
{
  meta::AbstractAttribute *p_attr = container.find(key);
  if (!p_attr)
    return;

  meta::qt::MetaWidget *widget = meta::qt::render_row(p_attr,
                                                      this->design,
                                                      this->ctx,
                                                      stage.row);
  if (!widget)
    return;

  if (const std::string tip = meta::common::try_get<std::string>(*p_attr,
                                                                 meta::keys::ui::tooltip,
                                                                 "");
      !tip.empty())
    widget->setToolTip(QString::fromStdString(tip));

  stage.row->content->addWidget(widget);
  stage.rows.push_back(widget);

  const PostEffect effect = stage.effect;

  QObject::connect(widget,
                   &meta::qt::MetaWidget::edit_started,
                   this->q,
                   [this, effect]() { this->begin_stage_edit(effect); });

  QObject::connect(widget,
                   &meta::qt::MetaWidget::value_changed,
                   this->q,
                   [this, effect]()
                   {
                     this->refresh_scope(effect);
                     Q_EMIT this->q->value_changed();
                   });

  QObject::connect(widget,
                   &meta::qt::MetaWidget::edit_ended,
                   this->q,
                   [this, effect]() { this->end_stage_edit(effect); });
}

void PostProcessDock::Impl::add_range(Stage                    &stage,
                                      meta::AttributeContainer &container,
                                      const char               *key)
{
  meta::AbstractAttribute *p_attr = container.find(key);
  auto *p_range = p_attr ? p_attr->try_cast<meta::Attribute<glm::vec2>>() : nullptr;
  if (!p_range)
    return;

  stage.range = new RangeEditor(*p_range, this->ctx, *this->theme, stage.row);

  if (const std::string tip = meta::common::try_get<std::string>(*p_attr,
                                                                 meta::keys::ui::tooltip,
                                                                 "");
      !tip.empty())
    stage.range->setToolTip(QString::fromStdString(tip));

  stage.row->content->addWidget(stage.range);

  const PostEffect effect = stage.effect;

  stage.range->on_edit_started = [this, effect]() { this->begin_stage_edit(effect); };
  stage.range->on_value_changed = [this, effect]()
  {
    this->refresh_scope(effect);
    Q_EMIT this->q->value_changed();
  };
  stage.range->on_edit_ended = [this, effect]() { this->end_stage_edit(effect); };
}

void PostProcessDock::Impl::begin_stage_edit(PostEffect effect)
{
  this->editing = effect;
  Q_EMIT this->q->edit_started();
}

void PostProcessDock::Impl::end_stage_edit(PostEffect effect)
{
  // A stage is taken out by removing it, never by dialling it back to a value
  // that does nothing: a Gamma dragged through 1, or reset to it with a
  // double-click, would otherwise vanish from under the pointer. Pin it while
  // this is still the same edit, so the pin lands in the same undo step.
  if (meta::AttributeContainer *p_container = this->container())
    if (!is_post_effect_active(*p_container, effect))
      set_post_effect_active(*p_container, effect, true);

  this->editing.reset();
  this->refresh_scope(effect);
  Q_EMIT this->q->edit_ended();
}

void PostProcessDock::Impl::set_effect(PostEffect effect, bool active)
{
  meta::AttributeContainer *p_container = this->container();
  Stage                    *p_stage = this->find(effect);
  if (!p_container || !p_stage)
    return;

  if (is_post_effect_active(*p_container, effect) != active)
  {
    Q_EMIT this->q->edit_started();
    set_post_effect_active(*p_container, effect, active);
  }
  else
  {
    // the model already agrees, only the dock was out of step
    this->set_shown(*p_stage, active, false);
    this->refresh_chain();
    return;
  }

  for (meta::qt::MetaWidget *widget : p_stage->rows)
    widget->sync_widget_from_model();
  if (p_stage->range)
    p_stage->range->sync();

  this->refresh_scope(effect);
  this->set_shown(*p_stage, active, /* animate */ true);
  this->refresh_chain();

  Q_EMIT this->q->value_changed();
  Q_EMIT this->q->edit_ended();
}

void PostProcessDock::Impl::set_shown(Stage &stage, bool shown, bool animate)
{
  stage.shown = shown;

  {
    const QSignalBlocker blocker(stage.chip);
    stage.chip->setChecked(shown);
  }

  stage.reveal->stop();

  if (!animate)
  {
    if (shown)
    {
      stage.clip->show();
      stage.clip->follow_body();
    }
    else
    {
      stage.clip->set_reveal(0);
      stage.clip->hide();
    }
    return;
  }

  const int from = stage.clip->isVisible() ? stage.clip->sizeHint().height() : 0;
  const int to = shown ? stage.clip->body_height() : 0;

  stage.clip->set_reveal(from);
  stage.clip->show();

  {
    // retargeting can emit a sample at the previous animation time
    const QSignalBlocker blocker(stage.reveal);
    stage.reveal->setStartValue(from);
    stage.reveal->setEndValue(to);
    stage.reveal->setCurrentTime(0);
  }

  stage.reveal->start();
}

void PostProcessDock::Impl::refresh_scope(PostEffect effect)
{
  const meta::AttributeContainer *p_container = this->container();
  Stage                          *p_stage = this->find(effect);

  if (p_container && p_stage)
    p_stage->row->scope->set_glyph(model_glyph(*p_container, effect));
}

void PostProcessDock::Impl::refresh_chain()
{
  Stage *p_first = nullptr;
  Stage *p_last = nullptr;

  for (Stage &stage : this->stages)
    if (stage.shown)
    {
      if (!p_first)
        p_first = &stage;
      p_last = &stage;
    }

  // the wire runs up from the tray through every stage and stops at the first
  for (Stage &stage : this->stages)
    stage.row->set_first(&stage == p_first);

  this->tray->set_live(p_last != nullptr);
  this->tray->refresh_hint();
}

void PostProcessDock::Impl::sync()
{
  BaseNode *p_node = this->resolver ? this->resolver() : nullptr;
  if (!p_node)
    return;

  meta::ContainerGroup &group = p_node->get_meta_group();

  if (group.current_container_name().value_or(std::string{}) != this->container_name)
  {
    this->rebuild();
    return;
  }

  const meta::AttributeContainer &container = group.current();

  for (Stage &stage : this->stages)
  {
    const bool active = is_post_effect_active(container, stage.effect);

    // Mid-drag the stage stays whatever the value passes through; see
    // end_stage_edit() for what happens when the drag ends on neutral.
    const bool held = this->editing && *this->editing == stage.effect;

    if (active != stage.shown && !held)
      this->set_shown(stage, active, /* animate */ this->q->isVisible());

    for (meta::qt::MetaWidget *widget : stage.rows)
      widget->sync_widget_from_model();
    if (stage.range)
      stage.range->sync();

    stage.row->scope->set_glyph(model_glyph(container, stage.effect));
  }

  this->refresh_chain();
}

PostProcessDock::PostProcessDock(NodeResolver                resolver,
                                 const std::string          &design,
                                 const meta::qt::RowContext &row_context,
                                 bool                        as_card,
                                 QWidget                    *parent)
    : QWidget(parent), impl(std::make_unique<Impl>())
{
  Logger::log()->trace("PostProcessDock::PostProcessDock");

  this->impl->q = this;
  this->impl->resolver = std::move(resolver);
  this->impl->design = design;
  this->impl->ctx = row_context;
  this->impl->theme = row_context.theme ? row_context.theme
                                        : &meta::qt::ThemeRegistry::instance().fallback();
  this->impl->as_card = as_card;

  this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

  // The pieces of the dock are QWidget subclasses without a meta-object of
  // their own, so to a stylesheet they are plain QWidgets, and the host's
  // background rule fills each one with the page colour: a dark slab over the
  // card. They all paint what they need themselves.
  this->setStyleSheet(".QWidget { background: transparent; }");

  auto *layout = new QVBoxLayout(this);
  layout->setSpacing(0);

  if (as_card)
  {
    // The insets a section card has, so the dock lines up with the ones above:
    // value fields end where theirs do, and the scopes sit in the column the
    // pin and the preset label start in.
    const meta::qt::Metrics &m = this->impl->theme->metrics;
    layout->setContentsMargins(m.section_card_margin + m.section_body_padding_x_narrow,
                               m.section_card_gap / 2 + 5,
                               m.section_card_margin + m.section_body_padding_x,
                               m.section_card_gap / 2 + 3);
  }
  else
  {
    layout->setContentsMargins(0, 4, 0, 4);
  }

  // stages first, the tray under them: see Tray for why it is the last row
  this->impl->stage_layout = new QVBoxLayout();
  this->impl->stage_layout->setContentsMargins(0, 0, 0, 0);
  this->impl->stage_layout->setSpacing(0);
  layout->addLayout(this->impl->stage_layout);

  this->impl->tray = new Tray(*this->impl->theme, this);
  layout->addWidget(this->impl->tray);

  this->impl->rebuild();
}

PostProcessDock::~PostProcessDock() = default;

bool PostProcessDock::is_needed(BaseNode &node)
{
  for (const auto &[name, p_container] : node.get_meta_group().containers())
    if (p_container && has_post_effects(*p_container))
      return true;

  return false;
}

void PostProcessDock::sync_from_model() { this->impl->sync(); }

void PostProcessDock::paintEvent(QPaintEvent *)
{
  if (!this->impl->as_card)
    return;

  const Theme             &theme = *this->impl->theme;
  const meta::qt::Metrics &m = theme.metrics;

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setPen(Qt::NoPen);
  painter.setBrush(theme.section_surface);
  painter.drawRoundedRect(this->rect().adjusted(m.section_card_margin,
                                                m.section_card_gap / 2,
                                                -m.section_card_margin,
                                                -m.section_card_gap / 2),
                          m.section_card_radius,
                          m.section_card_radius);
}

} // namespace hesiod
