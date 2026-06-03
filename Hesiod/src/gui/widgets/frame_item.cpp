/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QGraphicsSceneMouseEvent>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/coord_frame_widget.hpp"
#include "hesiod/logger.hpp"

namespace hesiod
{

FrameItem::FrameItem(const std::string &id, QPointF origin, QPointF size, float angle)
    : QGraphicsRectItem(), id(id), origin(origin), size(size), angle(angle)
{
  this->setFlag(QGraphicsItem::ItemIsSelectable, true);
  this->setFlag(QGraphicsItem::ItemIsMovable, true);
  this->setFlag(QGraphicsItem::ItemDoesntPropagateOpacityToChildren, false);
  this->setFlag(QGraphicsItem::ItemIsFocusable, true);
  this->setFlag(QGraphicsItem::ItemClipsChildrenToShape, false);
  this->setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
  this->setAcceptHoverEvents(true);

  this->setOpacity(0.7f);
  this->setZValue(0);

  this->update_item_geometry();
}

void FrameItem::get_frame_parameter(QPointF &origin_out,
                                    QPointF &size_out,
                                    float   &angle_out) const
{
  origin_out = this->origin;
  size_out = this->size;
  angle_out = this->angle;
}

void FrameItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
  this->is_hovered = true;
  this->set_is_rotated(false);
  this->set_is_resized(false);
  this->update();

  QGraphicsRectItem::hoverEnterEvent(event);
}

void FrameItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
  this->is_hovered = false;
  this->set_is_rotated(false);
  this->set_is_resized(false);
  this->update();

  QGraphicsRectItem::hoverLeaveEvent(event);
}

void FrameItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
  this->set_is_rotated(this->rotation_rect.contains(event->pos()));
  this->set_is_resized(this->resize_rect.contains(event->pos()));
}

QVariant FrameItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
  if (change == QGraphicsItem::ItemPositionChange)
  {
    this->origin = value.toPointF() / COORD_SCALE;
    Q_EMIT this->has_changed(this->id);
  }

  return QGraphicsRectItem::itemChange(change, value);
}

void FrameItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
  const QPointF pos = event->pos();
  const bool    left_button = event->buttons() & Qt::LeftButton;

  if (this->is_rotated && left_button)
  {
    // adjust the angle to center the rectangle on the mouse cursor
    const QPointF center = this->rotation_rect.center();

    float pos_angle = std::atan2(pos.y(), pos.x());
    float center_angle = std::atan2(center.y(), center.x());
    float delta = (pos_angle - center_angle) / M_PI * 180.f;

    this->set_angle(this->angle + delta);
  }
  else if (this->is_resized && left_button)
  {
    QPointF delta = pos - this->resize_rect.center();

    // 1:1 aspect ratio scaling if Shift
    if (event->modifiers() & Qt::ShiftModifier)
    {
      const float dmax = std::max(delta.x(), delta.y());
      delta = QPointF(dmax, dmax);
    }

    // prevent negative sizes
    QPointF new_size = this->size + delta / COORD_SCALE;
    new_size = QPointF(std::max(0.f, static_cast<float>(new_size.x())),
                       std::max(0.f, static_cast<float>(new_size.y())));

    this->set_size(new_size);
  }

  QGraphicsRectItem::mouseMoveEvent(event);
}

void FrameItem::paint(QPainter                       *painter,
                      const QStyleOptionGraphicsItem *option,
                      QWidget                        *widget)
{
  Q_UNUSED(option);
  Q_UNUSED(widget);

  QRect base_rect(0.f, 0.f, COORD_SCALE * this->size.x(), COORD_SCALE * this->size.y());

  // colors
  AppContext &ctx = HSD_CTX;

  QColor color_bg(ctx.app_settings.colors.bg_primary);
  QColor color_border(0, 0, 0, 255);
  QColor color_hovered(255, 255, 255, 255);

  // background
  if (!this->pixmap.isNull())
  {
    painter->save();
    painter->translate(0, this->rect().height());
    painter->scale(1.0, -1.0);
    painter->drawPixmap(this->rect(), this->pixmap, this->pixmap.rect());
    painter->restore();
  }
  else
  {
    painter->setBrush(QBrush(color_bg));
    painter->setPen(Qt::NoPen);
    painter->drawRect(base_rect);
  }

  // contour
  painter->setBrush(Qt::NoBrush);
  painter->setPen(QPen(is_hovered ? color_hovered : color_border, 1.f));
  painter->drawRect(base_rect);

  // corners
  painter->drawRect(this->rotation_rect);
  painter->drawRect(this->resize_rect);

  // text
  painter->scale(1.f, -1.f);
  QRect inv_rect(0.f,
                 -this->rect().height(),
                 this->rect().width(),
                 this->rect().height());
  painter->drawText(inv_rect, Qt::AlignLeft | Qt::AlignBottom, this->id.c_str());
}

void FrameItem::set_angle(float new_angle)
{
  this->angle = new_angle;
  this->update_item_geometry();
}

void FrameItem::set_background_image(QImage new_image)
{
  Logger::log()->trace("FrameItem::set_background_image: frame {}", this->id);

  if (new_image.isNull() || new_image.bits() == nullptr)
  {
    this->pixmap = QPixmap();
    return;
  }

  QImage safe_image = new_image.convertToFormat(QImage::Format_ARGB32);

  this->pixmap = QPixmap::fromImage(safe_image);
  this->pixmap = this->pixmap.transformed(QTransform().scale(1.f, -1.f));
  this->update();
};

void FrameItem::set_geometry(QPointF new_origin, QPointF new_size, float new_angle)
{
  this->origin = new_origin;
  this->size = new_size;
  this->angle = new_angle;
  this->update_item_geometry();
}

void FrameItem::set_is_resized(bool new_is_resized)
{
  this->is_resized = new_is_resized;
  this->update_cursor_and_flags();
}

void FrameItem::set_is_rotated(bool new_is_rotated)
{
  this->is_rotated = new_is_rotated;
  this->update_cursor_and_flags();
}

void FrameItem::set_origin(QPointF new_origin)
{
  this->origin = new_origin;
  this->update_item_geometry();
}

void FrameItem::set_size(QPointF new_size)
{
  this->size = new_size;
  this->update_item_geometry();
}

void FrameItem::set_z_depth(int new_z_depth)
{
  this->z_depth = new_z_depth;
  this->setZValue(static_cast<qreal>(this->z_depth));
  this->update();
}

void FrameItem::update_cursor_and_flags()
{
  if (is_resized)
  {
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setCursor(Qt::SizeBDiagCursor);
  }
  else if (is_rotated)
  {
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setCursor(Qt::CrossCursor);
  }
  else
  {
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setCursor(Qt::ArrowCursor);
  }
}

void FrameItem::update_item_geometry()
{
  this->setRect(0.f, 0.f, COORD_SCALE * this->size.x(), COORD_SCALE * this->size.y());
  this->setPos(COORD_SCALE * this->origin);
  this->setRotation(this->angle);
  this->setToolTip(QString::fromStdString(this->id));

  // corners (warning, y-scale is reversed...)
  this->rotation_rect = QRectF(this->rect().topRight() - QPointF(this->handle_size, 0.f),
                               QSizeF(this->handle_size, this->handle_size));

  this->resize_rect = QRectF(this->rect().bottomRight() -
                                 QPointF(this->handle_size, this->handle_size),
                             QSizeF(this->handle_size, this->handle_size));

  this->update();
  Q_EMIT this->has_changed(this->id);
}

} // namespace hesiod
