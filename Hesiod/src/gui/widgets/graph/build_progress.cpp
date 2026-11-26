/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include <QEnterEvent>
#include <QEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/graph/build_progress.hpp"
#include "hesiod/gui/widgets/graph/graph_style.hpp"
#include "hesiod/gui/widgets/rail/tool_rail.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"

namespace hesiod
{

namespace
{
const QString held_text = "Graph needs update";
const QString building_text = "Building";
} // namespace

BuildProgress::BuildProgress(QWidget *area, std::function<void()> resume)
    : QWidget(area->parentWidget()), area(area), resume(std::move(resume))
{
  this->setAttribute(Qt::WA_NoSystemBackground);
  this->setAttribute(Qt::WA_TranslucentBackground);

  QFont font = this->font();
  font.setBold(true);
  this->setFont(font);

  this->appear_t.on_tick = [this]()
  {
    const qreal t = std::clamp(this->appear_t.value(), 0.0, 1.0);
    const int   dy = qRound(graph_style::build_rise * (1.0 - t));
    this->move(this->x(), this->rest_y + dy);
    this->update();
  };
  this->appear_t.on_done = [this]()
  {
    if (!this->updating && this->appear_t.value() <= 0.005)
      this->hide();
  };

  this->hide();
  area->installEventFilter(this);

  // the toolbar of the editor, to keep clear of
  for (QWidget *child : area->findChildren<QWidget *>())
    if (auto *p_rail = dynamic_cast<rail::ToolRail *>(child))
    {
      this->rail = p_rail;
      p_rail->installEventFilter(this);
      break;
    }
}

void BuildProgress::on_update_started()
{
  // asked again while it runs (an edit): it goes on with what it had done
  if (!this->updating)
  {
    this->updating = true;
    this->done_ids.clear();
    this->pass.clear();
    this->done_before = this->done = this->total = 0;
    this->fill_t.set(0.0);
  }
  this->held = false;
  this->refresh();
}

void BuildProgress::on_update_steps(const QStringList &pass, int done)
{
  if (!this->updating)
    return;

  // a new pass, after an edit or a hold: what it computes again is not done any
  // more, so that no node is counted twice
  if (pass != this->pass)
  {
    this->pass = pass;
    for (const QString &id : pass)
      this->done_ids.remove(id);
    this->done_before = int(this->done_ids.size());
  }

  if (done > 0 && done <= pass.size())
    this->done_ids.insert(pass[done - 1]);

  this->done = this->done_before + done;
  this->total = this->done_before + int(pass.size());
  this->refresh();
}

void BuildProgress::on_update_held(bool held)
{
  if (!this->updating)
    return;

  this->held = held;
  this->refresh();
}

void BuildProgress::on_update_finished()
{
  this->updating = this->held = false;
  this->refresh();
}

bool BuildProgress::eventFilter(QObject *watched, QEvent *event)
{
  if ((watched == this->area || watched == this->rail.data()) &&
      (event->type() == QEvent::Resize || event->type() == QEvent::Move))
    this->place();
  return false;
}

void BuildProgress::enterEvent(QEnterEvent *event)
{
  if (this->held)
    this->hover_t.to(1.0, motion::hover_in, motion::enter());
  QWidget::enterEvent(event);
}

void BuildProgress::leaveEvent(QEvent *event)
{
  this->hover_t.to(0.0, motion::hover_out, motion::leave());
  QWidget::leaveEvent(event);
}

void BuildProgress::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton && this->held && this->resume)
    this->resume();
  event->accept();
}

void BuildProgress::paintEvent(QPaintEvent *)
{
  const auto &colors = HSD_CTX.app_settings.colors;

  const qreal vis = std::clamp(this->appear_t.value(), 0.0, 1.0);
  if (vis <= 0.002)
    return;

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setOpacity(vis);

  const QRectF box = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  const qreal  radius = box.height() / 2.0;
  QPainterPath shape;
  shape.addRoundedRect(box, radius, radius);

  const qreal h = this->held ? std::clamp(this->hover_t.value(), 0.0, 1.0) : 0.0;
  QColor      background = this->held ? mix_colors(colors.bg_deep,
                                              colors.warning,
                                              graph_style::build_hover_tint * h)
                                      : colors.bg_deep;
  background.setAlphaF(0.92);
  p.fillPath(shape, background);

  // the bar fills the pill itself, gliding smoothly between steps
  const qreal progress = std::clamp(this->fill_t.value(), 0.0, 1.0);
  if (!this->held && progress > 0.002)
  {
    QColor fill = colors.building;
    fill.setAlphaF(0.28);
    p.save();
    p.setClipPath(shape);
    p.fillRect(QRectF(box.left(), box.top(), box.width() * progress, box.height()), fill);
    p.restore();
  }

  QColor edge = this->held ? colors.warning : colors.building;
  edge.setAlphaF(0.75 + graph_style::build_edge_hover * h);
  p.setPen(QPen(edge, 1.0));
  p.drawPath(shape);

  const QRectF text_box = box.adjusted(radius, 0, -radius, 0);

  if (this->held)
  {
    p.setPen(mix_colors(colors.warning,
                        colors.text_primary,
                        graph_style::build_ink_hover * h));
    p.drawText(text_box, Qt::AlignCenter, held_text);
    return;
  }

  p.setPen(colors.text_primary);
  p.drawText(text_box, Qt::AlignLeft | Qt::AlignVCenter, building_text);

  if (this->total > 0)
  {
    p.setPen(colors.building);
    p.drawText(text_box,
               Qt::AlignRight | Qt::AlignVCenter,
               QString("%1 / %2").arg(this->done).arg(this->total));
  }
}

void BuildProgress::place()
{
  if (!this->area)
    return;

  // as wide as the old status bar one, or as its text needs
  const QFontMetrics fm(this->font());
  const int          h = 2 * fm.height();
  const int          text = std::max(fm.horizontalAdvance(held_text),
                            fm.horizontalAdvance(building_text + "  000 / 000"));
  const int          w = std::min(
      std::max(HSD_CTX.app_settings.window.progress_bar_width, text + 2 * h),
      this->area->width() - 2 * h);

  const QRect a = this->area->geometry();
  int         y = a.bottom() - h - h / 2;

  // above the toolbar when it is docked to the bottom
  if (this->rail && this->rail->get_edge() == rail::Edge::Bottom)
  {
    const QPoint offset = this->rail->parentWidget()->mapTo(this->parentWidget(),
                                                            QPoint(0, 0));
    const QRect  docked = this->rail->docked_rect().toAlignedRect().translated(offset);
    y = std::min(y, docked.top() - h - h / 4);
  }

  this->rest_y = y;
  const int dy = qRound(graph_style::build_rise *
                        (1.0 - std::clamp(this->appear_t.value(), 0.0, 1.0)));
  this->setGeometry(a.center().x() - w / 2, this->rest_y + dy, w, h);
}

void BuildProgress::refresh()
{
  const bool visible = this->updating;

  // clickable only to go on with a held update
  this->setAttribute(Qt::WA_TransparentForMouseEvents, !this->held);
  this->setCursor(this->held ? Qt::PointingHandCursor : Qt::ArrowCursor);
  if (!this->held)
    this->hover_t.to(0.0, motion::hover_out, motion::leave());
  this->setToolTip(this->held ? "The last update was stopped: nodes after the one being "
                                "computed are not up to date. Click to go on with it, "
                                "any edit does too."
                              : QString());

  if (this->total > 0)
  {
    const double ratio = std::clamp(double(this->done) / double(this->total), 0.0, 1.0);
    this->fill_t.to(ratio, motion::build_fill, motion::enter());
  }

  if (visible)
  {
    if (!this->isVisible())
    {
      this->appear_t.set(0.0);
      this->place();
      this->show();
    }
    this->raise();
    this->appear_t.to(1.0, motion::popup_in, motion::enter());
  }
  else if (this->isVisible())
  {
    if (this->total > 0)
      this->fill_t.to(1.0, motion::build_fill_last, motion::enter());
    this->appear_t.to(0.0, motion::popup_out, motion::leave());
  }

  this->update();
}

} // namespace hesiod
