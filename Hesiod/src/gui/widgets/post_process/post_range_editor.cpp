/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <any>

#include <QPainter>
#include <QResizeEvent>
#include <QSignalBlocker>

#include "meta/metadata/keys.hpp"
#include "meta_common.hpp"
#include "meta_qt/ui/theme.hpp"
#include "meta_qt/widgets/range_bar.hpp"

#include "hesiod/gui/widgets/post_process/post_dock_metrics.hpp"
#include "hesiod/gui/widgets/post_process/post_range_editor.hpp"

namespace hesiod
{

PostRangeEditor::PostRangeEditor(meta::Attribute<glm::vec2> &attr,
                                 const meta::qt::RowContext &ctx,
                                 const meta::qt::Theme      &theme,
                                 QWidget                    *parent)
    : QWidget(parent), attr(attr), ctx(ctx), theme(theme),
      label(QString::fromStdString(meta::common::label(attr))),
      vmin(meta::common::min<float>(attr)), vmax(meta::common::max<float>(attr))
{
  this->setFixedHeight(post_dock::RANGE_HEIGHT);
  this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  const int decimals = meta::common::try_get_format_decimals(meta::common::format(attr));

  this->bar = new meta::qt::RangeBar(attr.value(),
                                     this->vmin,
                                     this->vmax,
                                     decimals,
                                     this);
  this->bar->set_theme(theme);
  this->bar->setFixedHeight(post_dock::RANGE_HEIGHT);

  // the bar reads its readouts and histogram colours from the palette
  QPalette palette = this->bar->palette();
  palette.setColor(QPalette::Text, theme.ink_secondary);
  palette.setColor(QPalette::Dark, theme.rail_well_border);
  palette.setColor(QPalette::Highlight, theme.accent);
  this->bar->setPalette(palette);

  if (const auto *p_meta = attr.metadata().find(meta::keys::ui::data_provider))
    if (const auto *p_provider = p_meta->try_cast<meta::Attribute<meta::DataProvider>>())
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

void PostRangeEditor::commit(glm::vec2 range)
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

void PostRangeEditor::sync()
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

QSize PostRangeEditor::sizeHint() const
{
  return QSize(this->theme.metrics.label_min_width + 200, this->height());
}

void PostRangeEditor::resizeEvent(QResizeEvent *event)
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
    this->bar->setGeometry(-5, LABEL_LINE, this->width() + 10, post_dock::RANGE_HEIGHT);
  else
    this->bar->setGeometry(beside,
                           0,
                           this->width() - beside + 5,
                           post_dock::RANGE_HEIGHT);

  // where the stage's scope should sit: level with the label
  const int label_centre = this->stacked ? LABEL_LINE / 2
                                         : (post_dock::RANGE_HEIGHT - 8) / 2;
  if (this->property(post_dock::HEAD_CENTRE_PROPERTY) != QVariant(label_centre))
  {
    this->setProperty(post_dock::HEAD_CENTRE_PROPERTY, label_centre);
    this->updateGeometry(); // has the stage lay out again, and read it
  }

  this->setFixedHeight(this->stacked ? LABEL_LINE + post_dock::RANGE_HEIGHT
                                     : post_dock::RANGE_HEIGHT);
  QWidget::resizeEvent(event);
}

void PostRangeEditor::paintEvent(QPaintEvent *)
{
  QPainter painter(this);
  painter.setFont(meta::qt::row_label_font());
  painter.setPen(this->theme.state_ink(this->modified, false));

  // beside the bar, centred on its track, which it draws above its middle
  const QRect cell = this->stacked
                         ? QRect(0, 0, this->width(), LABEL_LINE)
                         : QRect(0, 0, this->label_width(), post_dock::RANGE_HEIGHT - 8);

  painter.drawText(
      cell,
      Qt::AlignLeft | Qt::AlignVCenter,
      painter.fontMetrics().elidedText(this->label, Qt::ElideRight, cell.width()));
}

int PostRangeEditor::label_width() const
{
  const meta::qt::Metrics &m = this->theme.metrics;
  return int(std::clamp<qreal>(this->width() * m.label_width_ratio,
                               m.label_min_width,
                               m.label_max_width));
}

void PostRangeEditor::remember(const glm::vec2 &range)
{
  meta::AttributeContainer &state = this->attr.state();
  if (auto *p_last = state.try_value<glm::vec2>(meta::keys::state::last_active_value))
    *p_last = range;
  else
    state.try_add(meta::keys::state::last_active_value, glm::vec2(range));
}

void PostRangeEditor::refresh_modified()
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

} // namespace hesiod
