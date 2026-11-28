/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cmath>
#include <functional>
#include <set>
#include <utility>

#include <QAbstractItemView>
#include <QApplication>
#include <QCompleter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QResizeEvent>
#include <QScrollArea>
#include <QStandardItemModel>
#include <QTimer>
#include <QVBoxLayout>

#include "hesiod/gui/widgets/node_widgets/node_palette/category_glyph.hpp"
#include "hesiod/gui/widgets/node_widgets/node_palette/fold_button.hpp"
#include "hesiod/gui/widgets/node_widgets/node_palette/fold_flight.hpp"
#include "hesiod/gui/widgets/node_widgets/node_palette/node_palette_style.hpp"
#include "hesiod/gui/widgets/node_widgets/node_palette/palette_flyout_card.hpp"
#include "hesiod/gui/widgets/node_widgets/node_palette/palette_tree_node.hpp"
#include "hesiod/gui/widgets/node_widgets/node_palette/rail_scroller.hpp"
#include "hesiod/gui/widgets/node_widgets/node_palette/search_fade_veil.hpp"
#include "hesiod/gui/widgets/node_widgets/node_palette_sidebar.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"

namespace hesiod
{

namespace np = node_palette;

namespace
{

/// Sidebars alive in this process, so a settings change can reach open windows.
std::set<NodePaletteSidebar *> &live_sidebars()
{
  static std::set<NodePaletteSidebar *> instances;
  return instances;
}

QStringList split_path(const QString &path)
{
  return path.split('/', Qt::SkipEmptyParts);
}

/** @brief A readable accent for a category that the palette does not name.
 *
 * Hashing the name keeps it stable across runs and across machines, which
 * matters because the accent is the only thing distinguishing two rail buttons
 * at a glance. Saturation and lightness are pinned so no category can come out
 * muddy or neon.
 */
QColor derived_accent(const QString &category)
{
  uint hash = 2166136261u;
  for (const QChar c : category)
    hash = (hash ^ static_cast<uint>(c.unicode())) * 16777619u;

  return QColor::fromHsl(static_cast<int>(hash % 360u), 110, 150);
}

/// The search field's leading mark, drawn at 3x so it stays crisp at any
/// interface scale.
QIcon magnifier_icon(const QColor &color)
{
  const qreal ratio = np::search_icon_ratio;
  QPixmap pixmap(int(np::search_icon_side * ratio), int(np::search_icon_side * ratio));
  pixmap.setDevicePixelRatio(ratio);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);
  QPen pen(color);
  pen.setWidthF(np::search_icon_pen);
  pen.setCapStyle(Qt::RoundCap);
  painter.setPen(pen);
  painter.drawEllipse(QPointF(np::search_icon_center, np::search_icon_center),
                      np::search_icon_radius,
                      np::search_icon_radius);
  painter.drawLine(QPointF(np::search_icon_handle_from, np::search_icon_handle_from),
                   QPointF(np::search_icon_handle_to, np::search_icon_handle_to));
  return QIcon(pixmap);
}

} // namespace

NodePaletteSidebar::NodePaletteSidebar(
    const std::map<std::string, std::string> &inventory,
    const std::map<std::string, QColor>      &category_colors,
    const NodePalettePalette                 &style,
    QWidget                                  *parent)
    : QWidget(parent), category_colors(category_colors), style(style)
{
  live_sidebars().insert(this);

  auto *layout = new QVBoxLayout(this);
  layout->setSpacing(this->style.rail_padding); // margins: see set_style

  // header: laid out by hand (layout_header) so the search field slides
  // behind the fold button as the rail folds, instead of being squeezed
  this->header_bar = new QWidget(this);
  this->header_bar->setAttribute(Qt::WA_NoSystemBackground);
  this->header_bar->setAutoFillBackground(false);

  this->search_clip = new QWidget(this->header_bar);
  this->search_clip->setAttribute(Qt::WA_NoSystemBackground);
  this->search_clip->setAutoFillBackground(false);

  this->search = new QLineEdit(this->search_clip);
  this->search->setPlaceholderText("Search nodes");
  this->search->setClearButtonEnabled(true);
  this->search_icon = this->search->addAction(magnifier_icon(this->style.text),
                                              QLineEdit::LeadingPosition);

  this->search_veil = new SearchFadeVeil(this->search_clip);

  this->fold_button = new FoldButton(this->header_bar);
  layout->addWidget(this->header_bar);

  this->connect(this->fold_button,
                &QAbstractButton::clicked,
                this,
                [this]()
                {
                  const bool unfolding = this->compact;
                  this->set_compact(!this->compact);
                  if (!unfolding || !this->search)
                    return;

                  // while the fold is in flight the field is not on screen
                  // yet; it takes the caret when it lands
                  if (this->flying)
                    this->search_wants_focus = true;
                  else
                    this->search->setFocus(Qt::OtherFocusReason);
                });

  this->reveal_anim.on_tick = [this]() { this->apply_reveal(this->reveal_anim.value()); };
  this->visibility_anim.on_tick = [this]()
  { this->apply_visibility(this->visibility_anim.value()); };
  this->visibility_anim.on_done = [this]()
  {
    if (!this->target_visible && this->visibility_anim.value() <= np::hidden_below)
      this->hide();
  };

  this->build_rail(inventory);
  this->setup_search(inventory);

  // the rail scrolls so every category stays reachable when the interface is
  // scaled up or the window is short
  this->scroll = new QScrollArea(this);
  this->scroll->setWidget(this->rail);
  this->scroll->setWidgetResizable(true);
  this->scroll->setFrameShape(QFrame::NoFrame);
  this->scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  // its own bar would stand in the rows, against the icons: see RailScroller
  this->scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  layout->addWidget(this->scroll, 100);
  this->scroller = new RailScroller(this->scroll, this);

  // The list is as tall as its rows and no taller (see set_style); whatever
  // height the editor has beyond that stays empty below the card, so the
  // library is a card that ends with its last row rather than a full-height
  // slab with a hole at the bottom.
  layout->addStretch(1);

  this->setToolTip("Click a category for its nodes.\n"
                   "Ctrl + click a node: replace the selected node\n"
                   "Shift + click a node: chain to the selected node");

  this->set_style(this->style);
}

NodePaletteSidebar::~NodePaletteSidebar()
{
  this->watch_application(false);
  if (this->flyout)
    delete this->flyout;
  if (this->flight)
    delete this->flight;
  live_sidebars().erase(this);
}

QColor NodePaletteSidebar::accent_for(const QString &category) const
{
  const auto it = this->category_colors.find(category.toStdString());
  if (it != this->category_colors.end() && it->second.isValid())
  {
    QColor color = it->second;
    color.setAlpha(255);

    // several registry colors are near-black, which is invisible as a glyph on
    // a dark rail; lift only those rather than recoloring the whole palette
    if (color.lightness() < 70)
      color = mix_colors(color, QColor("#c8c8c8"), 0.55);

    return color;
  }

  return derived_accent(category);
}

std::vector<std::string> NodePaletteSidebar::all_node_types() const
{
  std::vector<std::string> all;
  for (const auto &types : this->node_types)
    all.insert(all.end(), types.begin(), types.end());

  std::sort(all.begin(), all.end());
  all.erase(std::unique(all.begin(), all.end()), all.end());
  return all;
}

void NodePaletteSidebar::build_rail(const std::map<std::string, std::string> &inventory)
{
  // node type -> "Cat/Sub/Sub" becomes a per-category tree; the rail is the top
  // level, everything below it is a flyout
  struct TreeNode
  {
    std::map<QString, TreeNode> children;
    std::vector<QString>        leaves;
  };

  std::map<QString, TreeNode> roots;

  for (const auto &[node_type, path] : inventory)
  {
    const QStringList parts = split_path(QString::fromStdString(path));
    if (parts.isEmpty())
      continue;

    TreeNode *cursor = &roots[parts.front()];
    for (int i = 1; i < parts.size(); ++i)
      cursor = &cursor->children[parts.at(i)];

    cursor->leaves.push_back(QString::fromStdString(node_type));
  }

  this->rail = new QWidget;
  auto *rail_layout = new QVBoxLayout(this->rail);
  rail_layout->setContentsMargins(0, 0, 0, 0);
  rail_layout->setSpacing(this->style.button_spacing);

  // what a category's card lists, built recursively so a category three
  // levels deep is not a special case
  std::function<std::shared_ptr<PaletteTreeNode>(const QString &,
                                                 const QString &,
                                                 const QString &,
                                                 const TreeNode &,
                                                 const QColor &)>
      build_tree = [&](const QString  &title,
                       const QString  &path,
                       const QString  &category,
                       const TreeNode &node,
                       const QColor   &accent)
  {
    auto out = std::make_shared<PaletteTreeNode>();
    out->title = title;
    out->path = path;
    out->category = category;
    out->accent = accent;
    out->leaves = node.leaves;
    std::sort(out->leaves.begin(), out->leaves.end());
    for (const auto &[name, child] : node.children)
      out->children.push_back(
          build_tree(name, path + "/" + name, category, child, accent));
    return out;
  };

  for (const auto &[category, tree] : roots)
  {
    const QColor accent = this->accent_for(category);

    auto *button = new CategoryRailButton(category, accent, this->style, this->rail);
    rail_layout->addWidget(button);

    // collect every leaf under this category, for the tests and for the search
    std::vector<std::string>              types;
    std::function<void(const TreeNode &)> collect = [&](const TreeNode &node)
    {
      for (const QString &leaf : node.leaves)
        types.push_back(leaf.toStdString());
      for (const auto &[_, child] : node.children)
        collect(child);
    };
    collect(tree);
    std::sort(types.begin(), types.end());

    const int index = static_cast<int>(this->buttons.size());

    this->buttons.push_back(button);
    this->trees.push_back(build_tree(category, category, category, tree, accent));
    this->node_types.push_back(std::move(types));

    this->connect(button,
                  &QAbstractButton::clicked,
                  this,
                  [this, index]()
                  {
                    if (this->open_index == index)
                      this->close_flyout();
                    else
                      this->open_category(index);
                  });

    this->connect(button,
                  &CategoryRailButton::hovered,
                  this,
                  [this, index]()
                  {
                    // only follow the pointer once the palette is already open,
                    // so a stray pass over the rail does not spawn a flyout
                    if (this->open_index >= 0 && this->open_index != index)
                      this->open_category(index);
                  });
  }

  rail_layout->addStretch(1);
}

CategoryRailButton *NodePaletteSidebar::button_at(int index) const
{
  if (index < 0 || index >= static_cast<int>(this->buttons.size()))
    return nullptr;
  return this->buttons.at(index);
}

QRectF NodePaletteSidebar::card_rect() const
{
  int bottom = this->height() - np::outer;
  if (this->scroll)
    bottom = std::min(bottom,
                      this->scroll->geometry().bottom() + 1 + this->style.rail_padding);

  return QRectF(np::outer + 0.5,
                np::outer + 0.5,
                this->width() - np::outer - 1.0,
                bottom - np::outer - 1.0);
}

int NodePaletteSidebar::category_count() const
{
  return static_cast<int>(this->buttons.size());
}

QStringList NodePaletteSidebar::category_names() const
{
  QStringList names;
  for (CategoryRailButton *button : this->buttons)
    names << button->text();
  return names;
}

void NodePaletteSidebar::close_flyout()
{
  if (this->open_index < 0)
    return;

  const int index = this->open_index;
  this->open_index = -1;

  this->watch_application(false);

  if (this->flyout)
    this->flyout->dismiss();

  if (CategoryRailButton *button = this->button_at(index))
    button->set_open(false);
}

int NodePaletteSidebar::compact_width() const
{
  // one square cell
  return this->style.button_height + 2 * this->style.rail_padding + np::outer;
}

void NodePaletteSidebar::emit_for_type(const QString &node_type)
{
  this->close_flyout();

  const std::string type = node_type.toStdString();
  const auto        modifiers = QApplication::keyboardModifiers();

  if (modifiers & Qt::ControlModifier)
    Q_EMIT this->node_type_selected_ctrl(type);
  else if (modifiers & Qt::ShiftModifier)
    Q_EMIT this->node_type_selected_shift(type);
  else
    Q_EMIT this->node_type_selected(type);
}

bool NodePaletteSidebar::eventFilter(QObject *watched, QEvent *event)
{
  // floating over the graph, the card follows the graph's size
  if (this->overlay_mode && watched == this->parentWidget() &&
      (event->type() == QEvent::Resize || event->type() == QEvent::Show))
    this->sync_overlay_bounds();

  if (this->open_index < 0)
    return QWidget::eventFilter(watched, event);

  // the card never takes the focus, so the application is watched while it is
  // open: a click elsewhere, Escape or the window going inactive closes it
  switch (event->type())
  {
  case QEvent::MouseButtonPress:
  case QEvent::MouseButtonDblClick:
  {
    const QPoint global = static_cast<QMouseEvent *>(event)->globalPosition().toPoint();
    const bool   on_rail = this->isVisible() &&
                         this->rect().contains(this->mapFromGlobal(global));
    const bool on_card = this->flyout && this->flyout->contains_global(global);
    if (!on_rail && !on_card)
      this->close_flyout();
    break;
  }

  case QEvent::KeyPress:
  {
    const int key = static_cast<QKeyEvent *>(event)->key();
    if (key == Qt::Key_Escape)
    {
      this->close_flyout();
      return true;
    }
    if (this->flyout && this->flyout->key(key))
      return true;
    break;
  }

  case QEvent::ApplicationDeactivate:
    this->close_flyout();
    break;

  case QEvent::WindowDeactivate:
    if (watched == this->window())
      this->close_flyout();
    break;

  default:
    break;
  }

  return QWidget::eventFilter(watched, event);
}

int NodePaletteSidebar::expanded_width() const
{
  int widest = 0;
  for (const CategoryRailButton *button : this->buttons)
    widest = std::max(widest, button->sizeHint().width());
  return std::max(widest, np::rows_min_width) + 2 * this->style.rail_padding + np::outer;
}

// The fold as one moving picture (see FoldFlight). False when it cannot be
// done that way (not floating over the graph, or nothing to draw on), and the
// caller folds the widgets live instead.
bool NodePaletteSidebar::fly_fold(double target)
{
  QWidget *host = this->parentWidget();
  if (!this->overlay_mode || !host || !host->isVisible() || this->visibility < 0.995)
    return false;

  // a flight still under way ends where it was going: the widgets under it
  // are already there
  this->land_fold();
  this->reveal_anim.set(this->reveal);

  QWidget *mate = (this->companion && this->companion->isVisible())
                      ? this->companion.data()
                      : nullptr;

  auto content_shot = [this]()
  {
    this->content_only = true;
    QPixmap shot = this->grab();
    this->content_only = false;
    return shot;
  };
  auto mate_rect = [host, mate]()
  { return mate ? QRect(mate->mapTo(host, QPoint(0, 0)), mate->size()) : QRect(); };

  // --- before
  const QPixmap content_from = content_shot();
  const QRect   side_from = this->geometry();
  const QRectF  card_from = this->card_rect().translated(side_from.topLeft());
  const QRect   mate_from = mate_rect();
  const QPixmap mate_shot = mate ? mate->grab() : QPixmap();
  const double  reveal_from = this->reveal;

  // --- after: the real widgets take their final state now, in one step
  this->reveal_anim.set(target);
  this->apply_reveal(target);

  const QPixmap content_to = content_shot();
  const QRect   side_to = this->geometry();
  const QRectF  card_to = this->card_rect().translated(side_to.topLeft());
  const QRect   mate_to = mate_rect();

  // --- the graph behind them, taken with both out of the way
  QRect bounds = side_from.united(side_to);
  if (mate)
    bounds = bounds.united(mate_from).united(mate_to);
  bounds = bounds
               .adjusted(-np::flight_margin,
                         -np::flight_margin,
                         np::flight_margin,
                         np::flight_margin)
               .intersected(host->rect());
  if (bounds.isEmpty())
    return true; // already in the final state, nothing to show

  // They stay out of the way until the picture has landed: shown now, they
  // would repaint in their new state underneath it, which is exactly the work
  // the picture is there to keep out of the moving frames.
  this->hide();
  if (mate)
    mate->hide();
  const QPixmap backdrop = host->grab(bounds);
  this->flying = true;

  const QPointF    base = bounds.topLeft();
  FoldFlight::Shot from, to;
  from.content = content_from;
  from.origin = QPointF(side_from.topLeft()) - base;
  from.card = card_from.translated(-base);
  from.rail_pos = QPointF(mate_from.topLeft()) - base;
  from.reveal = reveal_from;
  to.content = content_to;
  to.origin = QPointF(side_to.topLeft()) - base;
  to.card = card_to.translated(-base);
  to.rail_pos = QPointF(mate_to.topLeft()) - base;
  to.reveal = target;

  FoldFlight::Look look;
  look.card_bg = this->style.rail_bg;
  look.seam = this->style.seam;
  look.radius = this->style.corner_radius + np::card_radius_extra;

  this->flight = new FoldFlight(host,
                                bounds,
                                backdrop,
                                from,
                                to,
                                mate_shot,
                                look,
                                [this]() { this->land_fold(); });
  return true;
}

int NodePaletteSidebar::footprint_right() const
{
  if (!this->overlay_mode)
    return 0;
  if (!this->target_visible && this->visibility <= np::hidden_below)
    return 0;

  const int    from = this->compact_width();
  const int    to = this->expanded_width();
  const double w = from + (to - from) * this->reveal;
  return std::max(0, static_cast<int>(std::lround(w * this->visibility)));
}

const NodePalettePalette &NodePaletteSidebar::get_style() const { return this->style; }

int NodePaletteSidebar::header_height() const
{
  // the field and the button are one row with the category rows' rhythm
  return std::max(np::header_min_height, this->style.button_height - 2);
}

int NodePaletteSidebar::ideal_overlay_height() const
{
  const int pad = this->style.rail_padding;
  const int list_h = (this->rail && this->rail->layout())
                         ? this->rail->layout()->sizeHint().height()
                         : 0;
  return 2 * np::outer + 3 * pad + this->header_height() + list_h + np::card_bottom_slack;
}

bool NodePaletteSidebar::is_compact() const { return this->compact; }

// The picture has arrived (or is cut short): the widgets, already in their
// final state, come back in its place.
void NodePaletteSidebar::land_fold()
{
  if (!std::exchange(this->flying, false))
    return;

  if (this->target_visible)
    this->show();
  if (this->companion)
  {
    this->companion->show();
    this->companion->raise();
  }
  if (this->target_visible)
    this->raise();

  if (this->flight)
  {
    this->flight->hide();
    this->flight->deleteLater();
    this->flight = nullptr;
  }

  if (std::exchange(this->search_wants_focus, false) && this->search && !this->compact)
    this->search->setFocus(Qt::OtherFocusReason);
}

void NodePaletteSidebar::layout_header()
{
  if (!this->header_bar || !this->fold_button || !this->search_clip || !this->search)
    return;

  const int header_h = this->header_height();
  const int bar_w = this->header_bar->width();
  const int fold_x = std::max(0, bar_w - header_h);
  this->fold_button->setGeometry(fold_x, 0, header_h, header_h);

  // the field keeps its unfolded width and is cut by the clip as the rail
  // folds, so its text never reflows on the way
  const int  clip_w = std::max(0, fold_x - np::header_gap);
  const int  full_search_w = std::max(np::search_min_width,
                                     this->expanded_width() - np::outer -
                                         2 * this->style.rail_padding - header_h -
                                         np::header_gap);
  const bool show_search = this->reveal > np::hidden_below && clip_w > 2;
  this->search_clip->setVisible(show_search);
  if (!show_search)
    return;

  this->search_clip->setGeometry(0, 0, clip_w, header_h);
  this->search->setGeometry(0, 0, full_search_w, header_h);
  if (this->search_veil)
  {
    this->search_veil->setGeometry(0, 0, clip_w, header_h);
    this->search_veil->set_state(
        this->style.rail_bg,
        1.0 - np::smoothstep(np::search_fade_from, np::search_fade_to, this->reveal));
    this->search_veil->raise();
  }
}

PaletteFlyoutCard *NodePaletteSidebar::flyout_card() const { return this->flyout.data(); }

std::vector<std::string> NodePaletteSidebar::node_types_at(int index) const
{
  if (index < 0 || index >= static_cast<int>(this->node_types.size()))
    return {};
  return this->node_types.at(index);
}

void NodePaletteSidebar::open_category(int index)
{
  CategoryRailButton *button = this->button_at(index);
  if (!button || index < 0 || index >= static_cast<int>(this->trees.size()))
    return;

  // from one category to the next the open card moves, it does not close
  if (this->open_index >= 0 && this->open_index != index)
    if (CategoryRailButton *previous = this->button_at(this->open_index))
      previous->set_open(false);

  this->open_index = index;
  button->set_open(true);

  if (!this->flyout)
    this->flyout = new PaletteFlyoutCard(
        this->style,
        [this](const QString &node_type) { this->emit_for_type(node_type); },
        this->window());

  this->watch_application(true);

  // beside the card's right edge, level with the row
  const int    card_right = static_cast<int>(std::lround(this->card_rect().right()));
  const QPoint anchor = this->mapToGlobal(
      QPoint(card_right + np::flyout_gap,
             button->mapTo(this, QPoint(0, button->height() / 2)).y()));
  this->flyout->present(this->trees.at(size_t(index)), anchor);
}

int NodePaletteSidebar::open_category_index() const { return this->open_index; }

void NodePaletteSidebar::paintEvent(QPaintEvent *)
{
  // a picture of the content alone is being taken, see fly_fold()
  if (this->content_only)
    return;

  // Floating over the graph the card is drawn on the graph itself (nothing
  // behind its rounded corners); in a layout it fills its ground first.
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  if (this->overlay_mode)
    painter.setOpacity(std::clamp(this->visibility * np::show_opacity_lead, 0.0, 1.0));
  else
    painter.fillRect(this->rect(), this->style.canvas_bg);

  const QRectF card = this->card_rect();
  if (card.width() <= 0.0 || card.height() <= 0.0)
    return;

  const double radius = std::min<double>(this->style.corner_radius +
                                             np::card_radius_extra,
                                         std::min(card.width(), card.height()) / 2.0);
  painter.setPen(QPen(this->style.seam, 1));
  painter.setBrush(this->style.rail_bg);
  painter.drawRoundedRect(card, radius, radius);
}

// The glyphs of the search results are pictures in the list's model, so they
// are drawn again whenever the colors change.
void NodePaletteSidebar::refresh_search_icons()
{
  if (!this->search_model)
    return;

  const qreal dpr = this->devicePixelRatioF();
  const int   side = np::search_result_icon;

  for (int row = 0; row < this->search_model->rowCount(); ++row)
  {
    QStandardItem *item = this->search_model->item(row);
    if (!item)
      continue;

    const QString category = item->data(np::search_category_role).toString();
    const QColor  ink = mix_colors(
        this->style.text,
        this->accent_for(category),
        std::clamp(this->style.accent_strength * np::flyout_rest_ink, 0.0, 1.0));

    QPixmap picture(QSize(side, side) * dpr);
    picture.setDevicePixelRatio(dpr);
    picture.fill(Qt::transparent);
    {
      QPainter painter(&picture);
      painter.setRenderHint(QPainter::Antialiasing, true);
      draw_node_glyph(painter,
                      QRectF(0, 0, side, side),
                      ink,
                      item->data(np::search_type_role).toString(),
                      category);
    }
    item->setIcon(QIcon(picture));
  }
}

void NodePaletteSidebar::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  this->layout_header();
}

void NodePaletteSidebar::restyle_all(const NodePalettePalette &style)
{
  for (NodePaletteSidebar *sidebar : live_sidebars())
    sidebar->set_style(style);
}

QLineEdit *NodePaletteSidebar::search_field() const { return this->search; }

void NodePaletteSidebar::set_all_compact(bool state)
{
  // only the one on screen needs to be seen folding
  for (NodePaletteSidebar *sidebar : live_sidebars())
    sidebar->set_compact(state, sidebar->isVisible());
}

void NodePaletteSidebar::set_compact(bool state, bool animate)
{
  if (this->compact == state)
    return;

  this->compact = state;
  this->close_flyout();

  if (this->fold_button)
    this->fold_button->set_look(this->style, state);

  // folded, the name moves into the tooltip
  for (CategoryRailButton *button : this->buttons)
    button->setToolTip(state ? button->text() : QString());

  const double target = state ? 0.0 : 1.0;

  if (!animate || !this->style.animations || this->style.animation_ms <= 0 ||
      !this->isVisible())
  {
    this->reveal_anim.set(target);
    this->apply_reveal(target);
  }
  else if (!this->fly_fold(target))
  {
    this->reveal_anim.to(target,
                         state ? motion::library_fold_live : motion::library_unfold_live,
                         state ? motion::change() : motion::enter());
  }

  Q_EMIT this->compact_changed(state);
}

void NodePaletteSidebar::set_companion(QWidget *widget) { this->companion = widget; }

void NodePaletteSidebar::set_overlay_mode(bool enabled)
{
  if (this->overlay_mode == enabled)
    return;

  this->overlay_mode = enabled;
  this->setAttribute(Qt::WA_TranslucentBackground, enabled);
  this->setAttribute(Qt::WA_NoSystemBackground, enabled);
  this->setAutoFillBackground(!enabled);

  if (QWidget *host = this->parentWidget())
  {
    if (enabled)
      host->installEventFilter(this);
    else
      host->removeEventFilter(this);
  }

  if (enabled)
  {
    this->setMinimumWidth(0);
    this->setMaximumWidth(QWIDGETSIZE_MAX);
    this->sync_overlay_bounds();
  }
  else
  {
    this->apply_reveal(this->reveal);
  }
  this->update();
}

void NodePaletteSidebar::set_style(const NodePalettePalette &new_style)
{
  this->style = new_style;

  if (auto *layout = qobject_cast<QVBoxLayout *>(this->layout()))
  {
    // the card is inset from the editor's edges on three sides; on the fourth
    // it meets the graph, where the tool rail keeps its own distance
    const int pad = this->style.rail_padding;
    layout->setContentsMargins(np::outer + pad, np::outer + pad, pad, np::outer + pad);
    layout->setSpacing(pad);
  }

  if (this->rail && this->rail->layout())
    this->rail->layout()->setSpacing(this->style.button_spacing);

  for (CategoryRailButton *button : this->buttons)
    button->set_style(this->style);

  if (this->flyout)
    this->flyout->set_style(this->style);
  this->refresh_search_icons();

  if (this->scroll)
  {
    if (this->rail && this->rail->layout())
      this->scroll->setMaximumHeight(this->rail->layout()->sizeHint().height());

    this->scroll->setStyleSheet(
        "QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; }");

    // the bar for the short editor that cannot show every category sits in
    // the card's edge padding and only exists while there is something to
    // scroll
    if (this->scroller)
    {
      this->scroller->set_colors(this->style.rail_bg, this->style.text);
      this->scroller->set_step(np::wheel_rows *
                               (this->style.button_height + this->style.button_spacing));
      this->scroller->sync();
    }
  }

  const int header_h = this->header_height();

  if (this->header_bar)
    this->header_bar->setFixedHeight(header_h);

  if (this->search)
  {
    // a well in the card: no border at rest, a gray one under the pointer and
    // the accent only while typing
    this->search->setFixedHeight(header_h);
    this->search->setStyleSheet(
        QString(
            "QLineEdit { background-color: %1; color: %2; border: 1px solid %1;"
            " border-radius: %3px; padding: 0px %4px; selection-background-color: %5; }"
            "QLineEdit:hover { border-color: %6; }"
            "QLineEdit:focus { border-color: %5; }")
            .arg(this->style.field_bg.name())
            .arg(this->style.text_active.name())
            .arg(std::max(0, std::min(this->style.corner_radius, header_h / 2)))
            .arg(np::search_padding)
            .arg(this->style.focus.name())
            .arg(this->style.flyout_border.name()));
    if (this->search_icon)
      this->search_icon->setIcon(magnifier_icon(this->style.text));
  }

  if (this->fold_button)
  {
    this->fold_button->setFixedSize(header_h, header_h);
    this->fold_button->set_look(this->style, this->compact);
    this->fold_button->set_reveal(this->reveal);
  }

  this->reveal_anim.set(this->compact ? 0.0 : 1.0);
  this->apply_reveal(this->compact ? 0.0 : 1.0);

  this->update();
}

void NodePaletteSidebar::set_visible_animated(bool visible, bool animate)
{
  this->land_fold();
  this->target_visible = visible;

  // a hidden rail must not leave its flyout floating over the graph
  if (!visible)
    this->close_flyout();

  if (!this->overlay_mode)
  {
    this->visibility = visible ? 1.0 : 0.0;
    this->visibility_anim.set(this->visibility);
    this->setVisible(visible);
    return;
  }

  const double target = visible ? 1.0 : 0.0;
  if (visible && !this->isVisible())
  {
    this->show();
    this->raise();
  }

  if (!animate || !this->style.animations || this->style.animation_ms <= 0 ||
      !this->parentWidget() || !this->parentWidget()->isVisible())
  {
    this->visibility_anim.set(target);
    this->apply_visibility(target);
    if (!visible)
      this->hide();
  }
  else
  {
    this->visibility_anim.to(target,
                             visible ? motion::library_show : motion::library_hide,
                             visible ? motion::enter() : motion::leave());
  }
}

void NodePaletteSidebar::setup_search(const std::map<std::string, std::string> &inventory)
{
  // a model rather than a plain list of strings, so every result carries its
  // node's glyph (see refresh_search_icons)
  this->search_model = new QStandardItemModel(this);

  for (const auto &[node_type, path] : inventory)
  {
    // the category is part of the searchable string so "erosion" finds the
    // whole family, not just the nodes with it in their name
    const QString display = QString("%1  ·  %2")
                                .arg(QString::fromStdString(node_type))
                                .arg(QString::fromStdString(path));

    auto *item = new QStandardItem(display);
    item->setData(QString::fromStdString(node_type), np::search_type_role);
    item->setData(split_path(QString::fromStdString(path)).value(0),
                  np::search_category_role);
    this->search_model->appendRow(item);
    this->search_index[display] = QString::fromStdString(node_type);
  }

  auto *completer = new QCompleter(this->search_model, this);
  completer->setCaseSensitivity(Qt::CaseInsensitive);
  completer->setFilterMode(Qt::MatchContains);
  completer->setCompletionMode(QCompleter::PopupCompletion);
  completer->setMaxVisibleItems(np::search_max_rows);
  // wide enough for a name and its category: at the width of the field, with
  // a glyph ahead of each one, every result was cut off after a few letters
  if (QAbstractItemView *popup = completer->popup())
    popup->setMinimumWidth(np::search_popup_min_width);

  this->search->setCompleter(completer);

  // QCompleter's popup brings arrow keys, Enter, Escape, scrolling and
  // screen-bounds handling with it, so none of that is reimplemented here
  this->connect(completer,
                QOverload<const QString &>::of(&QCompleter::activated),
                this,
                [this](const QString &display)
                {
                  const auto it = this->search_index.find(display);
                  if (it == this->search_index.end())
                    return;

                  const QString type = it->second;
                  this->search->clear();
                  this->emit_for_type(type);
                });

  // Enter without picking from the popup still works when the text names a node
  this->connect(this->search,
                &QLineEdit::returnPressed,
                this,
                [this]()
                {
                  const QString text = this->search->text().trimmed();
                  if (text.isEmpty())
                    return;

                  for (const auto &[display, type] : this->search_index)
                    if (type.compare(text, Qt::CaseInsensitive) == 0)
                    {
                      this->search->clear();
                      this->emit_for_type(type);
                      return;
                    }
                });
}

void NodePaletteSidebar::sync_overlay_bounds()
{
  if (!this->overlay_mode || !this->parentWidget())
    return;

  const int from = this->compact_width();
  const int to = this->expanded_width();
  const int w = static_cast<int>(std::lround(from + (to - from) * this->reveal));
  const int parent_h = this->parentWidget()->height();
  const int h = std::max(1, std::min(parent_h, this->ideal_overlay_height()));
  const int x = static_cast<int>(std::lround(-w * (1.0 - this->visibility)));

  this->setGeometry(x, 0, w, h);
  this->layout_header();
  Q_EMIT this->footprint_changed(this->footprint_right());
}

const PaletteTreeNode *NodePaletteSidebar::tree_at(int index) const
{
  if (index < 0 || index >= static_cast<int>(this->trees.size()))
    return nullptr;
  return this->trees.at(size_t(index)).get();
}

void NodePaletteSidebar::watch_application(bool state)
{
  if (state == this->watching_application || !qApp)
    return;

  if (state)
    qApp->installEventFilter(this);
  else
    qApp->removeEventFilter(this);
  this->watching_application = state;
}

void NodePaletteSidebar::apply_reveal(double amount)
{
  this->reveal = std::clamp(amount, 0.0, 1.0);

  if (this->overlay_mode)
  {
    this->sync_overlay_bounds();
  }
  else
  {
    const int from = this->compact_width();
    const int to = this->expanded_width();
    this->setFixedWidth(static_cast<int>(std::lround(from + (to - from) * this->reveal)));
    this->layout_header();
  }

  if (this->fold_button)
    this->fold_button->set_reveal(this->reveal);

  for (CategoryRailButton *button : this->buttons)
    button->set_reveal(this->reveal);

  this->update();
}

void NodePaletteSidebar::apply_visibility(double amount)
{
  this->visibility = std::clamp(amount, 0.0, 1.0);
  this->sync_overlay_bounds();
  this->update();
}

} // namespace hesiod
