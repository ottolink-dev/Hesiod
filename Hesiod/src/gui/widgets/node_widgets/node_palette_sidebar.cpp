/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cmath>
#include <functional>
#include <set>
#include <utility>

#include <QApplication>
#include <QCompleter>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollArea>
#include <QStringListModel>
#include <QTimer>
#include <QVBoxLayout>

#include "hesiod/gui/widgets/node_widgets/node_palette/fold_button.hpp"
#include "hesiod/gui/widgets/node_widgets/node_palette/fold_flight.hpp"
#include "hesiod/gui/widgets/node_widgets/node_palette/node_palette_style.hpp"
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

/** @brief Force a shown popup inside @p available.
 *
 * QMenu positions itself on the screen, but a menu with more entries than the
 * display is tall is laid out in columns first, and that layout overshoots by
 * the frame width -- measured at every scale from 0.5x to 3x, the last column
 * ended up 13 to 88 logical pixels below the bottom of the screen. Sliding the
 * popup back inside fixes that; its size is left to QMenu, whose own scroll
 * arrows (`menu-scrollable`) handle a list taller than the display.
 */
void clamp_to_screen(QMenu *menu, const QRect &available)
{
  // move() below produces a move event that comes straight back here; one pass
  // is always enough, so refuse to re-enter rather than relying on the
  // rectangle happening to be a fixed point
  static bool clamping = false;
  if (clamping)
    return;

  const QRect geom = menu->geometry();
  QRect       moved = geom;

  // Move only, never resize. A popup's size is decided by QMenu's own layout
  // pass (column count, scroll arrows, the native shadow margin around it);
  // forcing a different size from the outside leaves the widget's idea of its
  // rectangle and the platform window disagreeing, and every mouse position is
  // then translated through the wrong one -- the cursor highlights one entry
  // while the click lands on another. Anything genuinely taller than the screen
  // is handled by the menu's own scroll arrows, which is what `menu-scrollable`
  // in the stylesheet is for.
  if (moved.bottom() > available.bottom())
    moved.moveBottom(available.bottom());
  if (moved.right() > available.right())
    moved.moveRight(available.right());

  // top/left last: on a menu taller or wider than the screen these win, so the
  // start of the list stays reachable rather than its end
  if (moved.top() < available.top())
    moved.moveTop(available.top());
  if (moved.left() < available.left())
    moved.moveLeft(available.left());

  if (moved.topLeft() == geom.topLeft())
    return;

  clamping = true;
  menu->move(moved.topLeft());
  clamping = false;
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

void NodePaletteSidebar::apply_menu_style(QMenu *menu, const QColor &accent)
{
  const QString
      sheet = QString("QMenu {"
                      "  background-color: %1;"
                      "  color: %2;"
                      "  border: 1px solid %3;"
                      "  border-left: 2px solid %4;"
                      "  border-radius: %5px;"
                      "  padding: %6px;"
                      // Without this a category with more entries than the screen is
                      // tall gets laid out in multiple columns, which overshoots the
                      // screen by the frame and puts the last column off the edge. A
                      // scrolling single column is both correct and easier to read.
                      "  menu-scrollable: 1;"
                      "}"
                      "QMenu::item {"
                      "  background: transparent;"
                      "  height: %7px;"
                      "  padding-left: 14px;"
                      "  padding-right: 28px;"
                      "  border-radius: %8px;"
                      "}"
                      "QMenu::item:selected {"
                      "  background-color: %9;"
                      "  color: %10;"
                      "}"
                      "QMenu::item:disabled { color: %3; }"
                      "QMenu::separator {"
                      "  height: 1px;"
                      "  background: %3;"
                      "  margin: %6px 8px;"
                      "}"
                      "QMenu::icon { padding-left: 10px; }")
                  .arg(this->style.flyout_bg.name())
                  .arg(this->style.text.name())
                  .arg(this->style.flyout_border.name())
                  .arg(accent.name())
                  .arg(std::max(0, this->style.corner_radius))
                  .arg(std::max(0, this->style.flyout_padding))
                  .arg(std::max(12, this->style.flyout_row_height))
                  .arg(std::max(0, this->style.corner_radius / 2))
                  .arg(this->style.surface_selected.name())
                  .arg(this->style.text_active.name());

  menu->setStyleSheet(sheet);

  for (QAction *action : menu->actions())
    if (action->menu())
      this->apply_menu_style(action->menu(), accent);
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

  // recursive menu build, so a category three levels deep is not a special case
  std::function<void(QMenu *, const TreeNode &, const QColor &)> fill =
      [&](QMenu *menu, const TreeNode &node, const QColor &accent)
  {
    std::vector<QString> leaves = node.leaves;
    std::sort(leaves.begin(), leaves.end());

    for (const QString &leaf : leaves)
    {
      QAction *action = menu->addAction(leaf);
      action->setData(leaf);

      // connected per action rather than through QMenu::triggered: that signal
      // only reaches the root menu along the popup chain, so a node picked from
      // a submenu would depend on how the menu happened to be opened
      this->connect(action,
                    &QAction::triggered,
                    this,
                    [this, leaf]() { this->emit_for_type(leaf); });
    }

    if (!leaves.empty() && !node.children.empty())
      menu->addSeparator();

    for (const auto &[name, child] : node.children)
    {
      QMenu *sub = menu->addMenu(name.toUpper());
      fill(sub, child, accent);
    }
  };

  for (const auto &[category, tree] : roots)
  {
    const QColor accent = this->accent_for(category);

    auto *button = new CategoryRailButton(category, accent, this->style, this->rail);
    rail_layout->addWidget(button);

    auto *menu = new QMenu(category, this);
    fill(menu, tree, accent);
    this->apply_menu_style(menu, accent);

    // filter the whole tree, not just the root: submenus need the show-time
    // screen clamp, and mouse moves over them still have to switch categories
    std::function<void(QMenu *)> watch = [&](QMenu *m)
    {
      m->installEventFilter(this);
      for (QAction *action : m->actions())
        if (action->menu())
          watch(action->menu());
    };
    watch(menu);

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
    this->menus.push_back(menu);
    this->node_types.push_back(std::move(types));

    this->connect(menu,
                  &QMenu::aboutToHide,
                  this,
                  [this, index]()
                  {
                    if (this->open_index == index)
                      this->close_flyout();
                  });

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

  if (QMenu *menu = this->menu_at(index))
    menu->close();

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

  // Submenus are popped up by Qt, not by us, so this is the only place a deep
  // node list can be pulled back onto the screen. Move and Resize are watched
  // as well as Show because Qt keeps adjusting a scrollable menu after it has
  // been shown, and the last word has to be the screen's.
  if (event->type() == QEvent::Show || event->type() == QEvent::Move ||
      event->type() == QEvent::Resize)
  {
    if (auto *menu = qobject_cast<QMenu *>(watched))
      if (menu->isVisible())
        if (const QScreen *screen = menu->screen())
          clamp_to_screen(menu, screen->availableGeometry());
  }

  if (event->type() == QEvent::MouseMove && this->open_index >= 0 &&
      qobject_cast<QMenu *>(watched))
  {
    auto        *move = static_cast<QMouseEvent *>(event);
    const QPoint global = move->globalPosition().toPoint();

    for (int i = 0; i < static_cast<int>(this->buttons.size()); ++i)
    {
      CategoryRailButton *button = this->buttons.at(i);
      if (i == this->open_index || !button->isVisible())
        continue;

      if (button->rect().contains(button->mapFromGlobal(global)))
      {
        // reopening from inside the menu's own event delivery re-enters
        // QMenu::popup; defer it by one turn of the loop
        QTimer::singleShot(0, this, [this, i]() { this->open_category(i); });
        break;
      }
    }
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

QMenu *NodePaletteSidebar::menu_at(int index) const
{
  if (index < 0 || index >= static_cast<int>(this->menus.size()))
    return nullptr;
  return this->menus.at(index);
}

std::vector<std::string> NodePaletteSidebar::node_types_at(int index) const
{
  if (index < 0 || index >= static_cast<int>(this->node_types.size()))
    return {};
  return this->node_types.at(index);
}

void NodePaletteSidebar::open_category(int index)
{
  CategoryRailButton *button = this->button_at(index);
  QMenu              *menu = this->menu_at(index);

  if (!button || !menu)
    return;

  if (this->open_index >= 0 && this->open_index != index)
    this->close_flyout();

  this->open_index = index;
  button->set_open(true);

  // flush against the card's right edge: a gap here is the classic hover dead
  // zone, where the pointer leaves the rail before it reaches the flyout
  QPoint pos = this->mapToGlobal(
      QPoint(this->width(), button->mapTo(this, QPoint(0, 0)).y()));

  const QScreen *screen = button->screen();
  QRect          available;

  if (screen)
  {
    available = screen->availableGeometry();

    // nudge upwards up front, so the common case does not flip the whole menu
    // over the rail
    const int height = menu->sizeHint().height();
    if (pos.y() + height > available.bottom())
      pos.setY(std::max(available.top(), available.bottom() - height));
  }

  menu->popup(pos);

  if (!available.isNull())
    clamp_to_screen(menu, available);
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

  for (int i = 0; i < static_cast<int>(this->buttons.size()); ++i)
  {
    CategoryRailButton *button = this->buttons.at(i);
    button->set_style(this->style);

    if (QMenu *menu = this->menu_at(i))
      this->apply_menu_style(menu, this->accent_for(button->text()));
  }

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
  QStringList entries;

  for (const auto &[node_type, path] : inventory)
  {
    // the category is part of the searchable string so "erosion" finds the
    // whole family, not just the nodes with it in their name
    const QString display = QString("%1  ·  %2")
                                .arg(QString::fromStdString(node_type))
                                .arg(QString::fromStdString(path));

    entries << display;
    this->search_index[display] = QString::fromStdString(node_type);
  }

  auto *completer = new QCompleter(entries, this);
  completer->setCaseSensitivity(Qt::CaseInsensitive);
  completer->setFilterMode(Qt::MatchContains);
  completer->setCompletionMode(QCompleter::PopupCompletion);
  completer->setMaxVisibleItems(14);

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
