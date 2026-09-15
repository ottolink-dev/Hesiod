/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cmath>
#include <functional>
#include <set>

namespace
{
constexpr double kPi = 3.14159265358979323846;
}


#include <QApplication>
#include <QCompleter>
#include <QEnterEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QScrollArea>
#include <QStringListModel>
#include <QTimer>
#include <QVBoxLayout>

#include "hesiod/gui/widgets/node_palette_sidebar.hpp"

namespace hesiod
{

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

QColor blend(const QColor &a, const QColor &b, double t)
{
  t = std::clamp(t, 0.0, 1.0);
  return QColor(static_cast<int>(a.red() * (1.0 - t) + b.red() * t),
                static_cast<int>(a.green() * (1.0 - t) + b.green() * t),
                static_cast<int>(a.blue() * (1.0 - t) + b.blue() * t));
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

/** @brief One of eight line glyphs, picked deterministically from the name.
 *
 * Hesiod ships no per-category artwork and inventing forty SVGs for a togglable
 * sidebar is not worth it. Drawing them keeps the set consistent, scales with
 * the interface scale for free, and stays distinguishable because the shape is
 * a stable function of the category name.
 */
void draw_glyph(QPainter &painter, const QRectF &box, const QColor &color, int shape)
{
  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);

  QPen pen(color);
  pen.setWidthF(std::max(1.2, box.width() * 0.09));
  pen.setCapStyle(Qt::RoundCap);
  pen.setJoinStyle(Qt::RoundJoin);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);

  const QPointF c = box.center();
  const double  r = box.width() * 0.5;

  switch (shape % 8)
  {
  case 0: // hexagon
  {
    QPolygonF poly;
    for (int i = 0; i < 6; ++i)
    {
      const double a = kPi / 6.0 + i * kPi / 3.0;
      poly << QPointF(c.x() + r * std::cos(a), c.y() + r * std::sin(a));
    }
    painter.drawPolygon(poly);
    break;
  }
  case 1: // ring with a core
    painter.drawEllipse(c, r, r);
    painter.setBrush(color);
    painter.drawEllipse(c, r * 0.22, r * 0.22);
    break;
  case 2: // waves
  {
    QPainterPath path;
    path.moveTo(box.left(), c.y() - r * 0.35);
    path.cubicTo(box.left() + box.width() * 0.33,
                 c.y() - r * 1.1,
                 box.left() + box.width() * 0.66,
                 c.y() + r * 0.4,
                 box.right(),
                 c.y() - r * 0.35);
    painter.drawPath(path);
    painter.translate(0, r * 0.7);
    painter.drawPath(path);
    break;
  }
  case 3: // ridge / triangle
  {
    QPolygonF poly;
    poly << QPointF(box.left(), box.bottom()) << QPointF(c.x(), box.top())
         << QPointF(box.right(), box.bottom());
    painter.drawPolyline(poly);
    break;
  }
  case 4: // grid
    for (int i = 0; i <= 2; ++i)
    {
      const double t = box.left() + box.width() * i * 0.5;
      painter.drawLine(QPointF(t, box.top()), QPointF(t, box.bottom()));
      const double u = box.top() + box.height() * i * 0.5;
      painter.drawLine(QPointF(box.left(), u), QPointF(box.right(), u));
    }
    break;
  case 5: // droplet
  {
    QPainterPath path;
    path.moveTo(c.x(), box.top());
    path.quadTo(box.right(), c.y(), c.x(), box.bottom());
    path.quadTo(box.left(), c.y(), c.x(), box.top());
    painter.drawPath(path);
    break;
  }
  case 6: // scatter
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    for (const QPointF &p : {QPointF(-0.5, -0.4),
                             QPointF(0.45, -0.15),
                             QPointF(-0.15, 0.5),
                             QPointF(0.2, 0.2),
                             QPointF(-0.45, 0.1)})
      painter.drawEllipse(QPointF(c.x() + p.x() * r * 1.6, c.y() + p.y() * r * 1.6),
                          r * 0.16,
                          r * 0.16);
    break;
  default: // node / connector
    painter.drawLine(QPointF(box.left(), c.y()), QPointF(box.right(), c.y()));
    painter.drawEllipse(QPointF(box.left() + r * 0.3, c.y()), r * 0.28, r * 0.28);
    painter.drawEllipse(QPointF(box.right() - r * 0.3, c.y()), r * 0.28, r * 0.28);
    break;
  }

  painter.restore();
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

int glyph_shape_for(const QString &category)
{
  uint hash = 2166136261u;
  for (const QChar c : category)
    hash = (hash ^ static_cast<uint>(c.unicode())) * 16777619u;
  return static_cast<int>(hash % 8u);
}

} // namespace

// =====================================
// CategoryRailButton
// =====================================

CategoryRailButton::CategoryRailButton(const QString          &category,
                                       const QColor           &accent,
                                       const NodePaletteStyle &style,
                                       QWidget                *parent)
    : QAbstractButton(parent), category(category), accent(accent), style(style)
{
  this->setText(category);
  this->setCursor(Qt::PointingHandCursor);
  this->setFocusPolicy(Qt::TabFocus);
  this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  this->setAttribute(Qt::WA_Hover, true);

  this->surface = this->style.surface;

  this->surface_animation.setEasingCurve(QEasingCurve::OutCubic);
  this->connect(&this->surface_animation,
                &QVariantAnimation::valueChanged,
                this,
                [this](const QVariant &value)
                {
                  this->surface = value.value<QColor>();
                  this->update();
                });
}

QColor CategoryRailButton::current_surface() const { return this->surface; }

void CategoryRailButton::enterEvent(QEnterEvent *event)
{
  this->hovering = true;
  this->retarget_surface();
  Q_EMIT this->hovered();
  QAbstractButton::enterEvent(event);
}

bool CategoryRailButton::is_open() const { return this->open; }

void CategoryRailButton::leaveEvent(QEvent *event)
{
  this->hovering = false;
  this->retarget_surface();
  QAbstractButton::leaveEvent(event);
}

void CategoryRailButton::paintEvent(QPaintEvent *)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  const QRectF box = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);

  painter.setPen(Qt::NoPen);
  painter.setBrush(this->surface);
  painter.drawRoundedRect(box, this->style.corner_radius, this->style.corner_radius);

  // the accent only ever touches the glyph and a hairline, never the surface:
  // a full-width category colour is what makes the old tree hard to read
  if (this->open || this->hovering)
  {
    QPen pen(blend(this->accent, this->style.surface_selected, 0.35));
    pen.setWidthF(1.0);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(box, this->style.corner_radius, this->style.corner_radius);
  }

  const double icon = this->style.icon_size;
  const double pad = this->style.rail_padding;

  const QRectF glyph_box(box.left() + pad,
                         box.center().y() - icon * 0.5,
                         icon,
                         icon);

  const QColor glyph_color = blend(this->style.text,
                                   this->accent,
                                   this->style.accent_strength);
  draw_glyph(painter,
             glyph_box.adjusted(icon * 0.12, icon * 0.12, -icon * 0.12, -icon * 0.12),
             glyph_color,
             glyph_shape_for(this->category));

  QFont font = this->font();
  font.setBold(this->open);
  painter.setFont(font);
  painter.setPen(this->open || this->hovering ? this->style.text_active
                                              : this->style.text);

  const QRectF text_box(glyph_box.right() + pad,
                        box.top(),
                        box.right() - glyph_box.right() - 2.0 * pad,
                        box.height());

  const QString elided = QFontMetrics(font).elidedText(this->category,
                                                       Qt::ElideRight,
                                                       static_cast<int>(
                                                           text_box.width()));
  painter.drawText(text_box, Qt::AlignVCenter | Qt::AlignLeft, elided);
}

void CategoryRailButton::retarget_surface()
{
  const QColor target = this->target_surface();

  if (this->surface == target)
    return;

  if (!this->style.animations || this->style.animation_ms <= 0)
  {
    // "off" has to mean off *now*, not after the current fade finishes
    this->surface_animation.stop();
    this->surface = target;
    this->update();
    return;
  }

  this->surface_animation.stop();
  this->surface_animation.setDuration(this->style.animation_ms);
  this->surface_animation.setStartValue(this->surface);
  this->surface_animation.setEndValue(target);
  this->surface_animation.start();
}

void CategoryRailButton::set_open(bool state)
{
  if (this->open == state)
    return;

  this->open = state;
  this->retarget_surface();
  this->update();
}

void CategoryRailButton::set_style(const NodePaletteStyle &new_style)
{
  this->style = new_style;
  this->surface_animation.stop();
  this->surface = this->target_surface();
  this->updateGeometry();
  this->update();
}

QSize CategoryRailButton::sizeHint() const
{
  const int text_w = QFontMetrics(this->font()).horizontalAdvance(this->category);
  return QSize(3 * this->style.rail_padding + this->style.icon_size + text_w,
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

// =====================================
// NodePaletteSidebar
// =====================================

NodePaletteSidebar::NodePaletteSidebar(
    const std::map<std::string, std::string> &inventory,
    const std::map<std::string, QColor>      &category_colors,
    const NodePaletteStyle                   &style,
    QWidget                                  *parent)
    : QWidget(parent), category_colors(category_colors), style(style)
{
  live_sidebars().insert(this);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(this->style.rail_padding,
                             this->style.rail_padding,
                             this->style.rail_padding,
                             this->style.rail_padding);
  layout->setSpacing(this->style.button_spacing);

  this->search = new QLineEdit(this);
  this->search->setPlaceholderText("Search nodes...");
  this->search->setClearButtonEnabled(true);
  layout->addWidget(this->search);

  this->build_rail(inventory);
  this->setup_search(inventory);

  // the rail scrolls so every category stays reachable when the interface is
  // scaled up or the window is short
  auto *scroll = new QScrollArea(this);
  scroll->setWidget(this->rail);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  layout->addWidget(scroll, 1);

  this->setToolTip("Click a category for its nodes.\n"
                   "Ctrl + click a node: replace the selected node\n"
                   "Shift + click a node: chain to the selected node");

  this->set_style(this->style);
}

NodePaletteSidebar::~NodePaletteSidebar() { live_sidebars().erase(this); }

QColor NodePaletteSidebar::accent_for(const QString &category) const
{
  const auto it = this->category_colors.find(category.toStdString());
  if (it != this->category_colors.end() && it->second.isValid())
  {
    QColor color = it->second;
    color.setAlpha(255);

    // several registry colours are near-black, which is invisible as a glyph on
    // a dark rail; lift only those rather than recolouring the whole palette
    if (color.lightness() < 70)
      color = blend(color, QColor("#c8c8c8"), 0.55);

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
  const QString sheet =
      QString("QMenu {"
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

  int max_width = 0;

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
    std::vector<std::string> types;
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

    max_width = std::max(max_width, button->sizeHint().width());
  }

  rail_layout->addStretch(1);

  this->setMinimumWidth(max_width + 4 * this->style.rail_padding);
}

CategoryRailButton *NodePaletteSidebar::button_at(int index) const
{
  if (index < 0 || index >= static_cast<int>(this->buttons.size()))
    return nullptr;
  return this->buttons.at(index);
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

void NodePaletteSidebar::emit_for_type(const QString &node_type)
{
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

  if (event->type() == QEvent::MouseMove && this->open_index >= 0)
  {
    auto *move = static_cast<QMouseEvent *>(event);
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

const NodePaletteStyle &NodePaletteSidebar::get_style() const { return this->style; }

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

  // flush against the button's right edge: a gap here is the classic hover dead
  // zone, where the pointer leaves the button before it reaches the flyout
  QPoint pos = button->mapToGlobal(QPoint(button->width(), 0));

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

void NodePaletteSidebar::restyle_all(const NodePaletteStyle &style)
{
  for (NodePaletteSidebar *sidebar : live_sidebars())
    sidebar->set_style(style);
}

QLineEdit *NodePaletteSidebar::search_field() const { return this->search; }

void NodePaletteSidebar::set_style(const NodePaletteStyle &new_style)
{
  this->style = new_style;

  if (auto *layout = qobject_cast<QVBoxLayout *>(this->layout()))
  {
    layout->setContentsMargins(this->style.rail_padding,
                               this->style.rail_padding,
                               this->style.rail_padding,
                               this->style.rail_padding);
    layout->setSpacing(this->style.button_spacing);
  }

  if (this->rail && this->rail->layout())
    this->rail->layout()->setSpacing(this->style.button_spacing);

  int max_width = 0;

  for (int i = 0; i < static_cast<int>(this->buttons.size()); ++i)
  {
    CategoryRailButton *button = this->buttons.at(i);
    button->set_style(this->style);
    max_width = std::max(max_width, button->sizeHint().width());

    if (QMenu *menu = this->menu_at(i))
      this->apply_menu_style(menu, this->accent_for(button->text()));
  }

  if (this->search)
    this->search->setStyleSheet(
        QString("QLineEdit { background-color: %1; color: %2; border: 1px solid %3;"
                " border-radius: %4px; padding: 4px 8px; }")
            .arg(this->style.flyout_bg.name())
            .arg(this->style.text_active.name())
            .arg(this->style.flyout_border.name())
            .arg(std::max(0, this->style.corner_radius)));

  if (max_width > 0)
    this->setMinimumWidth(max_width + 4 * this->style.rail_padding);

  this->update();
}

void NodePaletteSidebar::setup_search(
    const std::map<std::string, std::string> &inventory)
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

} // namespace hesiod
