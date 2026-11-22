/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <optional>
#include <string>
#include <vector>

#include <QMenu>
#include <QPainter>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include "meta/core/attribute.hpp"
#include "meta/core/attribute_container.hpp"
#include "meta/core/container_group.hpp"
#include "meta/metadata/keys.hpp"
#include "meta_common.hpp"
#include "meta_qt/designs/industrial/section.hpp"
#include "meta_qt/meta_widget.hpp"
#include "meta_qt/ui/design_registry.hpp"
#include "meta_qt/ui/theme.hpp"

#include "hesiod/gui/widgets/post_process/post_effect_chip.hpp"
#include "hesiod/gui/widgets/post_process/post_effect_glyph.hpp"
#include "hesiod/gui/widgets/post_process/post_effect_tray.hpp"
#include "hesiod/gui/widgets/post_process/post_process_dock.hpp"
#include "hesiod/gui/widgets/post_process/post_range_editor.hpp"
#include "hesiod/gui/widgets/post_process/post_stage_label.hpp"
#include "hesiod/gui/widgets/post_process/post_stage_row.hpp"
#include "hesiod/gui/widgets/post_process/post_stage_scope.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

// =====================================
// PostProcessDock
// =====================================

struct PostProcessDock::Impl
{
  struct Stage
  {
    PostEffect                          effect;
    meta::qt::industrial::ClipBox      *clip = nullptr;
    PostStageRow                       *row = nullptr;
    PostEffectChip                     *chip = nullptr;
    QVariantAnimation                  *reveal = nullptr;
    PostRangeEditor                    *range = nullptr;
    std::vector<meta::qt::MetaWidget *> rows;
    bool                                shown = false;
  };

  PostProcessDock       *q = nullptr;
  NodeResolver           resolver;
  std::string            design;
  meta::qt::RowContext   ctx;
  const meta::qt::Theme *theme = nullptr;
  bool                   as_card = false;

  PostEffectTray *tray = nullptr;
  QVBoxLayout    *stage_layout = nullptr;

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
  stage.row = new PostStageRow(effect, *this->theme, nullptr);

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
        new PostStageLabel(QString::fromStdString(label), *this->theme, stage.row));
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

  PostRangeEditor *range = stage.range;
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

  stage.range = new PostRangeEditor(*p_range, this->ctx, *this->theme, stage.row);

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
    p_stage->row->scope->set_glyph(model_post_glyph(*p_container, effect));
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

    stage.row->scope->set_glyph(model_post_glyph(container, stage.effect));
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

  // stages first, the tray under them: see PostEffectTray for why it is the last row
  this->impl->stage_layout = new QVBoxLayout();
  this->impl->stage_layout->setContentsMargins(0, 0, 0, 0);
  this->impl->stage_layout->setSpacing(0);
  layout->addLayout(this->impl->stage_layout);

  this->impl->tray = new PostEffectTray(*this->impl->theme, this);
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

  const meta::qt::Theme   &theme = *this->impl->theme;
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
