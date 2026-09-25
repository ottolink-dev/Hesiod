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
#include "hesiod/gui/widgets/graph_tab_strip.hpp"
#include "hesiod/gui/widgets/window_chrome.hpp"

namespace hesiod
{

namespace
{

QColor mix(const QColor &a, const QColor &b, qreal t)
{
  t = std::clamp(t, 0.0, 1.0);
  return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                          a.greenF() + (b.greenF() - a.greenF()) * t,
                          a.blueF() + (b.blueF() - a.blueF()) * t,
                          a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

// the card's own border colour (PanelFrame): the current tab continues it
QColor card_border()
{
  const auto &colors = HSD_CTX.app_settings.colors;
  return mix(colors.bg_primary, colors.border, 0.38);
}

// what the node editor is filled with, so the current tab is the same surface
QColor editor_fill() { return GN_STYLE->viewer.color_bg; }

// strip space: x along the edge, y across it (0 away from the editor,
// kThickness at the editor, the last kOverlap of it over the card's border)
constexpr qreal kTopActive = 3.0;  // the current tab's outer edge
constexpr qreal kTopResting = 8.0; // the others'
constexpr qreal kRadius = 9.0;     // tab's outer corners
constexpr qreal kFoot = 8.0;       // concave curves into the editor's border
constexpr qreal kFirstX = PanelFrame::radius + 12.0; // clear of the card's corner
constexpr qreal kGap = 2.0;
constexpr qreal kBase = GraphTabStrip::kThickness - GraphTabStrip::kOverlap + 0.5;

// the resting tabs' band across the strip, which the "+" shares
constexpr qreal kRestTop = kTopResting;
constexpr qreal kRestBottom = kBase - 5.5;

bool animations_on() { return HSD_CTX.app_settings.interface.enable_ui_animations; }

// a tab's outline from its first foot, over its outer edge, to its second
// foot: open on the editor's side
QPainterPath tab_outline(const QRectF &r, qreal base)
{
  QPainterPath path;
  path.moveTo(r.left() - kFoot, base);
  path.quadTo(QPointF(r.left(), base), QPointF(r.left(), base - kFoot));
  path.lineTo(r.left(), r.top() + kRadius);
  path.quadTo(r.topLeft(), QPointF(r.left() + kRadius, r.top()));
  path.lineTo(r.right() - kRadius, r.top());
  path.quadTo(r.topRight(), QPointF(r.right(), r.top() + kRadius));
  path.lineTo(r.right(), base - kFoot);
  path.quadTo(QPointF(r.right(), base), QPointF(r.right() + kFoot, base));
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
    this->setFixedWidth(kThickness);
    this->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  }
  else
  {
    this->setFixedHeight(kThickness);
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  }

  // one ticker eases every tab towards its target (rise, settle, hover), so a
  // switch is a single smooth motion rather than a jump
  this->ticker = new QTimer(this);
  this->ticker->setInterval(16);
  QObject::connect(this->ticker, &QTimer::timeout, this, [this]() { this->step(); });
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
  return this->orient == Qt::Vertical ? QSize(kThickness, 400) : QSize(400, kThickness);
}

QSize GraphTabStrip::minimumSizeHint() const
{
  return this->orient == Qt::Vertical ? QSize(kThickness, 80) : QSize(80, kThickness);
}

void GraphTabStrip::set_tabs(const QStringList &names, int new_current)
{
  // keep the animation state of tabs that stay
  std::vector<Tab> next;
  for (const QString &name : names)
  {
    Tab tab;
    tab.name = name;
    for (const Tab &old : this->tabs)
      if (old.name == name)
      {
        tab.active = old.active;
        tab.hover = old.hover;
      }
    next.push_back(tab);
  }

  const bool first = this->tabs.empty();
  this->tabs = std::move(next);
  this->current = new_current;

  // nothing to animate from the first time
  if (first || !animations_on())
    for (int i = 0; i < int(this->tabs.size()); ++i)
      this->tabs[i].active = i == this->current ? 1.0 : 0.0;

  this->layout_tabs();
  this->ticker->start();
  this->update();
}

void GraphTabStrip::layout_tabs()
{
  const QFont        font = meta::qt::ui_font(12, true);
  const QFontMetrics fm(font);

  // natural lengths, shrunk evenly when they do not fit
  std::vector<qreal> sizes;
  qreal              total = 0.0;
  for (const Tab &tab : this->tabs)
  {
    const qreal w = std::clamp(qreal(fm.horizontalAdvance(tab.name)) + 46.0, 90.0, 220.0);
    sizes.push_back(w);
    total += w + kGap;
  }
  const qreal room = this->length() - kFirstX - 44.0; // "+" and a margin
  const qreal k = total > room && total > 0 ? std::max(0.35, room / total) : 1.0;

  qreal x = kFirstX;
  for (size_t i = 0; i < this->tabs.size(); ++i)
  {
    const qreal w = std::max(56.0, sizes[i] * k);
    this->tabs[i].rect = QRectF(x, kTopActive, w, kThickness - kTopActive);
    x += w + kGap;
  }
}

QRectF GraphTabStrip::plus_rect() const
{
  // a square in the resting tabs' band, centred on it
  const qreal side = 22.0;
  const qreal mid = (kRestTop + kRestBottom) / 2.0;
  const qreal x = this->tabs.empty() ? kFirstX : this->tabs.back().rect.right() + 8.0;
  return QRectF(x, mid - side / 2.0, side, side);
}

void GraphTabStrip::step()
{
  const qreal k = animations_on() ? 0.22 : 1.0; // per 16 ms: ~180 ms to settle
  bool        moving = false;

  const auto ease = [&](qreal &value, qreal target)
  {
    const qreal d = target - value;
    if (std::abs(d) < 0.004)
      value = target;
    else
    {
      value += d * k;
      moving = true;
    }
  };

  for (int i = 0; i < int(this->tabs.size()); ++i)
  {
    ease(this->tabs[i].active, i == this->current ? 1.0 : 0.0);
    ease(this->tabs[i].hover, i == this->hovered ? 1.0 : 0.0);
  }
  ease(this->plus_hover, this->hovered == -2 ? 1.0 : 0.0);

  this->update();
  if (!moving)
    this->ticker->stop();
}

int GraphTabStrip::tab_at(const QPointF &pos) const
{
  for (int i = 0; i < int(this->tabs.size()); ++i)
  {
    const QRectF r = this->tabs[i].rect;
    if (pos.x() >= r.left() && pos.x() < r.right() && pos.y() >= kTopActive)
      return i;
  }
  if (this->on_new && this->plus_rect().adjusted(-3, -3, 3, 3).contains(pos))
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
  this->hovered = -1;
  this->ticker->start();
  QWidget::leaveEvent(event);
}

void GraphTabStrip::mouseMoveEvent(QMouseEvent *event)
{
  const int index = this->tab_at(this->to_widget().map(event->position()));
  if (index != this->hovered)
  {
    this->hovered = index;
    this->setCursor(index == -1 ? Qt::ArrowCursor : Qt::PointingHandCursor);
    this->ticker->start();
  }
}

void GraphTabStrip::mousePressEvent(QMouseEvent *event)
{
  if (event->button() != Qt::LeftButton)
    return QWidget::mousePressEvent(event);

  const int index = this->tab_at(this->to_widget().map(event->position()));
  if (index == -2 && this->on_new)
    this->on_new();
  else if (index >= 0 && index != this->current && this->on_selected)
    this->on_selected(index);
}

void GraphTabStrip::contextMenuEvent(QContextMenuEvent *event)
{
  const int index = this->tab_at(this->to_widget().map(QPointF(event->pos())));
  if (index >= 0 && this->on_context_menu)
  {
    this->on_context_menu(index, event->globalPos());
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
  const qreal      end = kThickness; // the strip's edge on the card side
  const QColor     fill = editor_fill();
  const QColor     border = card_border();
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
  std::stable_sort(order.begin(),
                   order.end(),
                   [this](int a, int b)
                   { return this->tabs[a].active < this->tabs[b].active; });

  for (const int i : order)
  {
    const Tab  &tab = this->tabs[i];
    const qreal a = tab.active;
    const qreal h = tab.hover * (1.0 - a);

    // the tab rises as it becomes current (and a little on hover)
    const qreal  top = kTopResting + (kTopActive - kTopResting) * a - 1.5 * h;
    const QRectF r(tab.rect.left(), top, tab.rect.width(), end - top);

    p.setTransform(to_widget);

    // resting: a soft pill in the band; current: the editor's own surface
    if (a < 0.999)
    {
      const QRectF pill(r.left() + 2, top, r.width() - 4, kRestBottom - top);
      QColor       rest = mix(colors.bg_deep, colors.bg_primary, 0.45 + 0.45 * h);
      rest.setAlphaF(1.0 - a);
      p.setPen(Qt::NoPen);
      p.setBrush(rest);
      p.drawRoundedRect(pill, 7, 7);
    }

    if (a > 0.001)
    {
      // the feet land on the card's border line; under the tab itself that
      // line is covered, so tab and editor are one surface
      QPainterPath outline = tab_outline(r, kBase);
      QPainterPath body = outline;
      body.closeSubpath();

      QColor surface = fill;
      surface.setAlphaF(a);
      p.setPen(Qt::NoPen);
      p.setBrush(surface);
      p.drawPath(body);
      p.drawRect(QRectF(r.left() + 0.5, kBase - 1.0, r.width() - 1.0, end - kBase + 1.0));

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
      const qreal mid = (top + kBase) / 2.0;
      p.drawRoundedRect(QRectF(r.left() + 10, mid - 6, 3, 12), 1.5, 1.5);
    }

    // label
    const QColor ink = mix(mix(colors.bg_primary, colors.text_primary, 0.55 + 0.35 * h),
                           colors.text_primary,
                           a);
    const qreal  label_bottom = a > 0.5 ? kBase : kRestBottom;
    // centred in the tab (symmetric margins, clear of the accent mark)
    draw_label(QRectF(r.left() + 18, top, r.width() - 36, label_bottom - top),
               tab.name,
               ink);
  }

  // "+": a new graph, centred in the resting band
  if (this->on_new)
  {
    p.setTransform(to_widget);
    const QRectF plus = this->plus_rect();
    if (this->plus_hover > 0.001)
    {
      QColor hover = mix(colors.bg_deep, colors.bg_primary, 0.9);
      hover.setAlphaF(this->plus_hover);
      p.setPen(Qt::NoPen);
      p.setBrush(hover);
      p.drawRoundedRect(plus, 6, 6);
    }
    QPen pen(mix(colors.bg_primary, colors.text_primary, 0.55 + 0.4 * this->plus_hover),
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

QSize TabbedCard::strip_extent() const
{
  const int band = GraphTabStrip::kThickness - GraphTabStrip::kOverlap;
  return this->strip->orientation() == Qt::Vertical ? QSize(band, 0) : QSize(0, band);
}

QSize TabbedCard::sizeHint() const
{
  return this->card->sizeHint() + this->strip_extent();
}

QSize TabbedCard::minimumSizeHint() const
{
  return this->card->minimumSizeHint() + this->strip_extent();
}

void TabbedCard::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  const int band = GraphTabStrip::kThickness - GraphTabStrip::kOverlap;

  if (this->strip->orientation() == Qt::Vertical)
  {
    this->card->setGeometry(band, 0, std::max(0, this->width() - band), this->height());
    this->strip->setGeometry(0, 0, GraphTabStrip::kThickness, this->height());
  }
  else
  {
    this->card->setGeometry(0, band, this->width(), std::max(0, this->height() - band));
    this->strip->setGeometry(0, 0, this->width(), GraphTabStrip::kThickness);
  }
  this->strip->raise();
}

} // namespace hesiod
