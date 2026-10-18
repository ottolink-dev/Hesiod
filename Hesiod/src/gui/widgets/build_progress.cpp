/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include <QEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/build_progress.hpp"
#include "hesiod/gui/widgets/rail/tool_rail.hpp"

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

void BuildProgress::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton && this->held && this->resume)
    this->resume();
  event->accept();
}

void BuildProgress::paintEvent(QPaintEvent *)
{
  const auto &colors = HSD_CTX.app_settings.colors;

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  const QRectF box = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  const qreal  radius = box.height() / 2.0;
  QPainterPath shape;
  shape.addRoundedRect(box, radius, radius);

  QColor background = colors.bg_deep;
  background.setAlphaF(0.92);
  p.fillPath(shape, background);

  // the bar fills the pill itself
  if (!this->held && this->total > 0)
  {
    QColor fill = colors.building;
    fill.setAlphaF(0.28);
    p.save();
    p.setClipPath(shape);
    p.fillRect(QRectF(box.left(),
                      box.top(),
                      box.width() * this->done / this->total,
                      box.height()),
               fill);
    p.restore();
  }

  QColor edge = this->held ? colors.warning : colors.building;
  edge.setAlphaF(0.75);
  p.setPen(QPen(edge, 1.0));
  p.drawPath(shape);

  const QRectF text_box = box.adjusted(radius, 0, -radius, 0);

  if (this->held)
  {
    p.setPen(colors.warning);
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

  this->setGeometry(a.center().x() - w / 2, y, w, h);
}

void BuildProgress::refresh()
{
  const bool visible = this->updating;

  // clickable only to go on with a held update
  this->setAttribute(Qt::WA_TransparentForMouseEvents, !this->held);
  this->setCursor(this->held ? Qt::PointingHandCursor : Qt::ArrowCursor);
  this->setToolTip(this->held ? "The last update was stopped: nodes after the one being "
                                "computed are not up to date. Click to go on with it, "
                                "any edit does too."
                              : QString());

  if (visible && !this->isVisible())
  {
    this->place();
    this->show();
  }
  else if (!visible)
    this->hide();

  this->raise();
  this->update();
}

} // namespace hesiod
