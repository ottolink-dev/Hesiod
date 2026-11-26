/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include <algorithm>
#include <cmath>

#include <QContextMenuEvent>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QToolTip>

#include "gnodegui/style.hpp"
#include "meta_qt/ui/theme.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/containers/panel_frame.hpp"
#include "hesiod/gui/widgets/graph/graph_style.hpp"
#include "hesiod/gui/widgets/graph/graph_tab_strip.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"

namespace hesiod
{

namespace
{

// what the node editor is filled with, so the current tab is the same surface
QColor editor_fill() { return GN_STYLE->viewer.color_bg; }

// strip space: x along the edge, y across it (0 away from the editor,
// thickness at the editor, the last overlap of it over the card's border)
constexpr qreal tab_top_active = 3.0;  // the current tab's outer edge
constexpr qreal tab_top_resting = 8.0; // the others'
constexpr qreal tab_radius = 9.0;      // tab's outer corners
constexpr qreal tab_foot = 8.0;        // concave curves into the editor's border
constexpr qreal tab_first_x = PanelFrame::radius + 12.0; // clear of the card's corner
constexpr qreal tab_gap = 2.0;
constexpr qreal tab_base = GraphTabStrip::thickness - GraphTabStrip::overlap + 0.5;

// the resting tabs' band across the strip, which the "+" shares
constexpr qreal rest_top = tab_top_resting;
constexpr qreal rest_bottom = tab_base - 5.5;

// a tab's outline from its first foot, over its outer edge, to its second
// foot: open on the editor's side
QPainterPath tab_outline(const QRectF &r, qreal base)
{
  QPainterPath path;
  path.moveTo(r.left() - tab_foot, base);
  path.quadTo(QPointF(r.left(), base), QPointF(r.left(), base - tab_foot));
  path.lineTo(r.left(), r.top() + tab_radius);
  path.quadTo(r.topLeft(), QPointF(r.left() + tab_radius, r.top()));
  path.lineTo(r.right() - tab_radius, r.top());
  path.quadTo(r.topRight(), QPointF(r.right(), r.top() + tab_radius));
  path.lineTo(r.right(), base - tab_foot);
  path.quadTo(QPointF(r.right(), base), QPointF(r.right() + tab_foot, base));
  return path;
}

} // namespace

// =====================================
// GraphTabStrip
// =====================================

GraphTabStrip::GraphTabStrip(Qt::Orientation orientation, QWidget *parent)
    : QWidget(parent), orient(orientation)
{
  this->setObjectName("hsdGraphTabStrip");
  this->setAttribute(Qt::WA_NoSystemBackground);
  this->setAttribute(Qt::WA_TranslucentBackground);
  this->setAttribute(Qt::WA_Hover);
  this->setMouseTracking(true);

  if (this->orient == Qt::Vertical)
  {
    this->setFixedWidth(thickness);
    this->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  }
  else
  {
    this->setFixedHeight(thickness);
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  }
}

QTransform GraphTabStrip::to_widget() const
{
  return this->orient == Qt::Vertical ? QTransform(0, 1, 1, 0, 0, 0) : QTransform();
}

qreal GraphTabStrip::length() const
{
  return this->orient == Qt::Vertical ? this->height() : this->width();
}

QSize GraphTabStrip::sizeHint() const
{
  return this->orient == Qt::Vertical ? QSize(thickness, 400) : QSize(400, thickness);
}

QPainterPath GraphTabStrip::current_tab_path() const
{
  if (this->current < 0 || this->current >= int(this->tabs.size()))
    return {};

  // as painted, see paintEvent()
  const Tab  &tab = this->tabs[this->current];
  const qreal top = tab_top_resting +
                    (tab_top_active - tab_top_resting) * tab.active->value();
  const QRectF r(tab.rect.left(), top, tab.rect.width(), thickness - top);

  QPainterPath path = tab_outline(r, tab_base);
  path.lineTo(r.right() + tab_foot, thickness + tab_foot);
  path.lineTo(r.left() - tab_foot, thickness + tab_foot);
  path.closeSubpath();
  return this->to_widget().map(path);
}

QSize GraphTabStrip::minimumSizeHint() const
{
  // room for the current tab's full name, so it never shows as "g…"
  qreal len = 80.0;
  if (this->current >= 0 && this->current < int(this->tabs.size()))
    len = tab_first_x + this->natural_width(this->tabs[this->current].name) + 12.0;

  return this->orient == Qt::Vertical ? QSize(thickness, int(std::ceil(len)))
                                      : QSize(int(std::ceil(len)), thickness);
}

qreal GraphTabStrip::natural_width(const QString &name) const
{
  const QFontMetrics fm(meta::qt::ui_font(12, true));
  return std::clamp(qreal(fm.horizontalAdvance(name)) + 46.0, 90.0, 220.0);
}

void GraphTabStrip::set_tabs(const QStringList &names, int new_current)
{
  const bool first = this->tabs.empty();

  // keep the animation state of tabs that stay
  std::vector<Tab> next;
  next.reserve(names.size());
  for (int i = 0; i < names.size(); ++i)
  {
    const QString &name = names[i];
    double prev_active = (first || !motion::enabled()) ? (i == new_current ? 1.0 : 0.0)
                                                       : 0.0;
    double prev_hover = 0.0;
    for (const Tab &old : this->tabs)
      if (old.name == name)
      {
        prev_active = old.active->value();
        prev_hover = old.hover->value();
        break;
      }

    Tab tab;
    tab.name = name;
    tab.active = std::make_unique<Anim>(this, prev_active);
    tab.hover = std::make_unique<Anim>(this, prev_hover);
    next.push_back(std::move(tab));
  }

  this->tabs = std::move(next);
  this->current = new_current;
  if (this->hovered >= int(this->tabs.size()))
    this->hovered = -1;

  for (int i = 0; i < int(this->tabs.size()); ++i)
  {
    const bool is_cur = i == this->current;
    if (first || !motion::enabled())
      this->tabs[i].active->set(is_cur ? 1.0 : 0.0);
    else
      this->tabs[i].active->to(is_cur ? 1.0 : 0.0,
                               is_cur ? motion::tab_rise : motion::tab_settle,
                               is_cur ? motion::enter() : motion::change());
    this->tabs[i].hover->to(i == this->hovered ? 1.0 : 0.0,
                            i == this->hovered ? motion::hover_in : motion::hover_out,
                            i == this->hovered ? motion::enter() : motion::change());
  }

  this->layout_tabs();
  this->updateGeometry(); // the minimum length follows the current tab
  this->update();
}

void GraphTabStrip::layout_tabs()
{
  constexpr qreal tab_min_width = 56.0;

  std::vector<qreal> sizes;
  qreal              total = 0.0;
  for (const Tab &tab : this->tabs)
  {
    sizes.push_back(this->natural_width(tab.name));
    total += sizes.back() + tab_gap;
  }

  // everything at its natural length when it fits with the "+" (and a margin)
  this->plus_shown = total <= this->length() - tab_first_x - 44.0;

  if (!this->plus_shown && !sizes.empty())
  {
    // Short on room: the "+" goes (New graph is also in the tab and Graph
    // menus), the current tab keeps as much of its name as it can and the
    // others share what is left.
    const qreal room = this->length() - tab_first_x - 12.0;
    const int   n = int(sizes.size());
    const int   cur = std::clamp(this->current, 0, n - 1);
    const qreal others = total - sizes[cur] - tab_gap;

    sizes[cur] = std::clamp(room - (n - 1) * (tab_min_width + tab_gap),
                            tab_min_width,
                            sizes[cur]);
    const qreal k = others > 0 ? std::max(0.0, room - sizes[cur] - tab_gap) / others
                               : 1.0;
    for (int i = 0; i < n; ++i)
      if (i != cur)
        sizes[i] = std::max(tab_min_width, sizes[i] * std::min(1.0, k));
  }

  qreal x = tab_first_x;
  for (size_t i = 0; i < this->tabs.size(); ++i)
  {
    this->tabs[i].rect = QRectF(x, tab_top_active, sizes[i], thickness - tab_top_active);
    x += sizes[i] + tab_gap;
  }
}

QRectF GraphTabStrip::plus_rect() const
{
  // a square in the resting tabs' band, centred on it
  const qreal side = 22.0;
  const qreal mid = (rest_top + rest_bottom) / 2.0;
  const qreal x = this->tabs.empty() ? tab_first_x : this->tabs.back().rect.right() + 8.0;
  return QRectF(x, mid - side / 2.0, side, side);
}

void GraphTabStrip::retarget_hover(int next)
{
  if (next == this->hovered)
    return;

  const int prev = this->hovered;
  this->hovered = next;

  if (prev >= 0 && prev < int(this->tabs.size()))
    this->tabs[prev].hover->to(0.0, motion::hover_out, motion::change());
  else if (prev == -2)
    this->plus_hover.to(0.0, motion::hover_out, motion::change());

  if (next >= 0 && next < int(this->tabs.size()))
    this->tabs[next].hover->to(1.0, motion::hover_in, motion::enter());
  else if (next == -2)
    this->plus_hover.to(1.0, motion::hover_in, motion::enter());
}

int GraphTabStrip::tab_at(const QPointF &pos) const
{
  for (int i = 0; i < int(this->tabs.size()); ++i)
  {
    const QRectF r = this->tabs[i].rect;
    if (pos.x() >= r.left() && pos.x() < r.right() && pos.y() >= tab_top_active)
      return i;
  }
  if (this->on_new && this->plus_shown &&
      this->plus_rect().adjusted(-3, -3, 3, 3).contains(pos))
    return -2;
  return -1;
}

bool GraphTabStrip::event(QEvent *event)
{
  if (event->type() == QEvent::ToolTip)
  {
    const auto *help = static_cast<QHelpEvent *>(event);
    const int   index = this->tab_at(this->to_widget().map(QPointF(help->pos())));
    if (index >= 0)
      QToolTip::showText(help->globalPos(), this->tabs[index].name, this);
    else if (index == -2)
      QToolTip::showText(help->globalPos(), "New graph", this);
    else
      QToolTip::hideText();
    return true;
  }
  if (event->type() == QEvent::Resize)
    this->layout_tabs();
  return QWidget::event(event);
}

void GraphTabStrip::leaveEvent(QEvent *event)
{
  this->retarget_hover(-1);
  QWidget::leaveEvent(event);
}

void GraphTabStrip::mouseMoveEvent(QMouseEvent *event)
{
  const int index = this->tab_at(this->to_widget().map(event->position()));
  if (index != this->hovered)
  {
    this->retarget_hover(index);
    this->setCursor(index == -1 ? Qt::ArrowCursor : Qt::PointingHandCursor);
  }
}

void GraphTabStrip::mousePressEvent(QMouseEvent *event)
{
  if (event->button() != Qt::LeftButton)
    return QWidget::mousePressEvent(event);

  const int index = this->tab_at(this->to_widget().map(event->position()));
  this->pressed_index = index;
  if (index != -1)
    this->press_t.to(1.0, motion::press_in, motion::enter());

  if (index == -2 && this->on_new)
    std::function<void()>(this->on_new)(); // a copy: the owner may replace it
  else if (index >= 0 && index != this->current && this->on_selected)
    std::function<void(int)>(this->on_selected)(index);
}

void GraphTabStrip::mouseReleaseEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton)
    this->press_t.to(0.0, motion::press_out, motion::change());
  QWidget::mouseReleaseEvent(event);
}

void GraphTabStrip::contextMenuEvent(QContextMenuEvent *event)
{
  const int index = this->tab_at(this->to_widget().map(QPointF(event->pos())));
  if (index >= 0 && this->on_context_menu)
  {
    // called through a copy: the owner may replace the callback while it runs
    std::function<void(int, const QPoint &)>(this->on_context_menu)(index,
                                                                    event->globalPos());
    event->accept();
    return;
  }
  QWidget::contextMenuEvent(event);
}

void GraphTabStrip::paintEvent(QPaintEvent *)
{
  const auto &colors = HSD_CTX.app_settings.colors;

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  const QTransform to_widget = this->to_widget();
  const bool       vertical = this->orient == Qt::Vertical;
  const qreal      end = thickness; // the strip's edge on the card side
  const QColor     fill = editor_fill();
  const QColor     border = panel_border_color();
  const QFont      font = meta::qt::ui_font(12, true);
  p.setFont(font);
  const QFontMetrics fm(font);

  // a label inside `r` (strip space), turned to read top to bottom when the
  // strip is vertical
  const auto draw_label = [&](const QRectF &r, const QString &text, const QColor &ink)
  {
    p.save();
    p.setTransform(QTransform());
    p.setPen(ink);
    const QString shown = fm.elidedText(text, Qt::ElideRight, int(r.width()));
    if (vertical)
    {
      const QRectF on_screen = to_widget.mapRect(r);
      p.translate(on_screen.center());
      p.rotate(90.0);
      p.drawText(QRectF(-on_screen.height() / 2.0,
                        -on_screen.width() / 2.0,
                        on_screen.height(),
                        on_screen.width()),
                 Qt::AlignCenter,
                 shown);
    }
    else
      p.drawText(r, Qt::AlignCenter, shown);
    p.restore();
  };

  // back to front: resting tabs, then the one(s) rising into the editor
  std::vector<int> order(this->tabs.size());
  for (int i = 0; i < int(order.size()); ++i)
    order[i] = i;
  std::stable_sort(
      order.begin(),
      order.end(),
      [this](int a, int b)
      { return this->tabs[a].active->value() < this->tabs[b].active->value(); });

  for (const int i : order)
  {
    const Tab  &tab = this->tabs[i];
    const qreal a = std::clamp(tab.active->value(), 0.0, 1.0);
    const qreal h = std::clamp(tab.hover->value(), 0.0, 1.0) * (1.0 - a);
    const qreal held = i == this->pressed_index
                           ? std::clamp(this->press_t.value(), 0.0, 1.0)
                           : 0.0;

    // the tab rises as it becomes current (and a little on hover)
    const qreal top = tab_top_resting + (tab_top_active - tab_top_resting) * a - 1.5 * h +
                      graph_style::tab_press_sink * held * (1.0 - a);
    const QRectF r(tab.rect.left(), top, tab.rect.width(), end - top);

    p.setTransform(to_widget);

    // resting: a soft pill in the band; current: the editor's own surface
    if (a < 0.999)
    {
      const QRectF pill(r.left() + 2, top, r.width() - 4, rest_bottom - top);
      QColor       rest = mix_colors(colors.bg_deep,
                               colors.bg_primary,
                               0.45 + 0.45 * h + graph_style::tab_press_tone * held);
      rest.setAlphaF(1.0 - a);
      p.setPen(Qt::NoPen);
      p.setBrush(rest);
      p.drawRoundedRect(pill, 7, 7);
    }

    if (a > 0.001)
    {
      // the feet land on the card's border line; under the tab itself that
      // line is covered, so tab and editor are one surface
      QPainterPath outline = tab_outline(r, tab_base);
      QPainterPath body = outline;
      body.closeSubpath();

      QColor surface = fill;
      surface.setAlphaF(a);
      p.setPen(Qt::NoPen);
      p.setBrush(surface);
      p.drawPath(body);
      p.drawRect(
          QRectF(r.left() + 0.5, tab_base - 1.0, r.width() - 1.0, end - tab_base + 1.0));

      QColor edge = border.lighter(118);
      edge.setAlphaF(a);
      p.setPen(QPen(edge, 1.0));
      p.setBrush(Qt::NoBrush);
      p.drawPath(outline);

      // accent mark at the tab's start
      QColor mark = colors.accent;
      mark.setAlphaF(a);
      p.setPen(Qt::NoPen);
      p.setBrush(mark);
      const qreal mid = (top + tab_base) / 2.0;
      p.drawRoundedRect(QRectF(r.left() + 10, mid - 6, 3, 12), 1.5, 1.5);
    }

    // label
    const QColor ink = mix_colors(
        mix_colors(colors.bg_primary, colors.text_primary, 0.55 + 0.35 * h),
        colors.text_primary,
        a);
    const qreal label_bottom = a > 0.5 ? tab_base : rest_bottom;
    // centred in the tab (symmetric margins, clear of the accent mark)
    draw_label(QRectF(r.left() + 18, top, r.width() - 36, label_bottom - top),
               tab.name,
               ink);
  }

  // "+": a new graph, centred in the resting band
  if (this->on_new && this->plus_shown)
  {
    p.setTransform(to_widget);
    const qreal  ph = std::clamp(this->plus_hover.value(), 0.0, 1.0);
    const qreal  pheld = this->pressed_index == -2
                             ? std::clamp(this->press_t.value(), 0.0, 1.0)
                             : 0.0;
    const qreal  shrink = graph_style::plus_press_inset * pheld;
    const QRectF plus = this->plus_rect().adjusted(shrink, shrink, -shrink, -shrink);
    if (ph > 0.001 || pheld > 0.001)
    {
      QColor hover = mix_colors(colors.bg_deep,
                                colors.bg_primary,
                                0.9 + graph_style::plus_press_tone * pheld);
      hover.setAlphaF(std::max(ph, pheld));
      p.setPen(Qt::NoPen);
      p.setBrush(hover);
      p.drawRoundedRect(plus, 6, 6);
    }
    QPen pen(mix_colors(colors.bg_primary,
                        colors.text_primary,
                        0.55 + 0.4 * std::max(ph, pheld)),
             1.5);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    const QPointF c = plus.center();
    p.drawLine(c + QPointF(-4.5, 0), c + QPointF(4.5, 0));
    p.drawLine(c + QPointF(0, -4.5), c + QPointF(0, 4.5));
  }
}

// =====================================
// TabbedCard
// =====================================

TabbedCard::TabbedCard(QWidget *card, GraphTabStrip *strip, QWidget *parent)
    : QWidget(parent), card(card), strip(strip)
{
  this->setObjectName("hsdBase");
  card->setParent(this);
  strip->setParent(this);
  strip->raise(); // over the card's border
  this->setSizePolicy(card->sizePolicy());
}

QPainterPath TabbedCard::outline(qreal inset) const
{
  const QRectF card = QRectF(this->card->geometry())
                          .adjusted(inset, inset, -inset, -inset);
  const qreal radius = std::max(0.0, PanelFrame::radius - inset);

  QPainterPath path;
  path.addRoundedRect(card, radius, radius);

  QPainterPath tab = this->strip->current_tab_path().translated(this->strip->pos());
  tab = tab.subtracted(QPainterPathStroker(QPen(Qt::black, 2 * inset)).createStroke(tab));

  // a single closed line: a dash pattern starts over with each piece of a path
  QPolygonF line;
  for (const QPolygonF &polygon : path.united(tab).simplified().toFillPolygons())
    if (polygon.size() > line.size())
      line = polygon;

  QPainterPath outline;
  outline.addPolygon(line);
  outline.closeSubpath();
  return outline;
}

QSize TabbedCard::strip_extent() const
{
  const int band = GraphTabStrip::thickness - GraphTabStrip::overlap;
  return this->strip->orientation() == Qt::Vertical ? QSize(band, 0) : QSize(0, band);
}

QSize TabbedCard::sizeHint() const
{
  return this->card->sizeHint() + this->strip_extent();
}

QSize TabbedCard::minimumSizeHint() const
{
  // along the strip, at least what its current tab needs
  QSize       size = this->card->minimumSizeHint() + this->strip_extent();
  const QSize strip = this->strip->minimumSizeHint();
  return this->strip->orientation() == Qt::Vertical
             ? QSize(size.width(), std::max(size.height(), strip.height()))
             : QSize(std::max(size.width(), strip.width()), size.height());
}

void TabbedCard::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  const int band = GraphTabStrip::thickness - GraphTabStrip::overlap;

  if (this->strip->orientation() == Qt::Vertical)
  {
    this->card->setGeometry(band, 0, std::max(0, this->width() - band), this->height());
    this->strip->setGeometry(0, 0, GraphTabStrip::thickness, this->height());
  }
  else
  {
    this->card->setGeometry(0, band, this->width(), std::max(0, this->height() - band));
    this->strip->setGeometry(0, 0, this->width(), GraphTabStrip::thickness);
  }
  this->strip->raise();
}

} // namespace hesiod
