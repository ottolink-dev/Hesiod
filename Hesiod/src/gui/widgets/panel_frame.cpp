/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/gui/widgets/panel_frame.hpp"

namespace hesiod
{

namespace
{

QPainterPath panel_path(const QSize &size)
{
  QPainterPath path;
  path.addRoundedRect(QRectF(0.5, 0.5, size.width() - 1.0, size.height() - 1.0),
                      PanelFrame::radius,
                      PanelFrame::radius);
  return path;
}

// One of the four corner caps of a PanelFrame. Covers the square corner of
// whatever the panel hosts with the background colour and redraws the rounded
// border stroke on top, so the corner is round regardless of what is below.
class PanelCorner final : public QWidget
{
public:
  explicit PanelCorner(PanelFrame *panel) : QWidget(panel), panel(panel)
  {
    this->setAttribute(Qt::WA_TransparentForMouseEvents);
    this->setAttribute(Qt::WA_NoSystemBackground);
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setFixedSize(PanelFrame::radius + 1, PanelFrame::radius + 1);
  }

protected:
  void paintEvent(QPaintEvent *) override
  {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.translate(-this->pos());

    // The shapes only change with the card's size or this corner's place, but
    // a card like the viewport repaints continuously (camera drags): rebuild
    // them, and the path subtraction, only then.
    if (this->panel->size() != this->cached_size || this->pos() != this->cached_pos)
    {
      this->cached_size = this->panel->size();
      this->cached_pos = this->pos();
      this->rounded = panel_path(this->cached_size);
      QPainterPath square;
      square.addRect(QRectF(this->geometry()));
      this->outside = square.subtracted(this->rounded);
    }

    p.fillPath(this->outside, HSD_CTX.app_settings.colors.bg_deep);
    p.setPen(QPen(panel_border_color(), 1));
    p.setBrush(Qt::NoBrush);
    p.drawPath(this->rounded);
  }

private:
  PanelFrame  *panel;
  QSize        cached_size;
  QPoint       cached_pos;
  QPainterPath rounded;
  QPainterPath outside;
};

} // namespace

// =====================================
// PanelFrame
// =====================================

PanelFrame::PanelFrame(QWidget *content, QWidget *parent)
    : QWidget(parent), p_content(content)
{
  this->setObjectName("hsdPanelFrame");
  this->setAttribute(Qt::WA_StyledBackground, false);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(1, 1, 1, 1);
  layout->setSpacing(0);

  // an explicitly hidden pane stays hidden, and so does its card
  const bool content_hidden = content &&
                              content->testAttribute(Qt::WA_WState_ExplicitShowHide) &&
                              content->testAttribute(Qt::WA_WState_Hidden);

  if (content)
  {
    // keep the size constraints of the pane on the card, so splitters and
    // layouts treat the card exactly as they treated the bare pane
    this->setSizePolicy(content->sizePolicy());
    layout->addWidget(content);
    content->installEventFilter(this);
  }

  for (auto &corner : this->corners)
    corner = new PanelCorner(this);

  this->installEventFilter(this);

  if (content_hidden)
    this->hide();
}

bool PanelFrame::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == this->p_content)
  {
    if (event->type() == QEvent::HideToParent)
      this->hide();
    else if (event->type() == QEvent::ShowToParent)
      this->show();
  }
  else if (watched == this && event->type() == QEvent::ChildPolished)
  {
    // Anything added later (overlays, popups parented here) must not end up
    // above the corner caps. ChildPolished comes once the child is fully built
    // and before it is first shown, so the caps go back on top right there,
    // with no deferred call to race widget rebuilds.
    for (auto *corner : this->corners)
      if (corner && static_cast<QChildEvent *>(event)->child() != corner)
        corner->raise();
  }

  return QWidget::eventFilter(watched, event);
}

void PanelFrame::paintEvent(QPaintEvent *)
{
  const auto &colors = HSD_CTX.app_settings.colors;

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.fillRect(this->rect(), colors.bg_deep);
  p.setPen(QPen(panel_border_color(), 1));
  p.setBrush(colors.bg_primary);
  p.drawPath(panel_path(this->size()));
}

void PanelFrame::place_corners()
{
  const int s = PanelFrame::radius + 1;
  const int w = this->width();
  const int h = this->height();

  if (!this->corners[0])
    return;

  this->corners[0]->move(0, 0);
  this->corners[1]->move(w - s, 0);
  this->corners[2]->move(w - s, h - s);
  this->corners[3]->move(0, h - s);

  for (auto *corner : this->corners)
  {
    corner->raise();
    corner->update();
  }
}

void PanelFrame::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  this->place_corners();
}

} // namespace hesiod
