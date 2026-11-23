/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QStyleOptionGraphicsItem>

#include "highmap/math/constants.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/graph/coord_frame_widget.hpp"
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
    QPointF new_pos = value.toPointF();
    if (this->snap_enabled && !(QApplication::keyboardModifiers() & Qt::AltModifier))
    {
      const qreal snap = 0.25 * COORD_SCALE;
      new_pos.setX(std::round(new_pos.x() / snap) * snap);
      new_pos.setY(std::round(new_pos.y() / snap) * snap);
    }
    this->origin = new_pos / COORD_SCALE;
    Q_EMIT this->has_changed(this->id);
    return new_pos;
  }
  else if (change == QGraphicsItem::ItemSelectedHasChanged)
  {
    this->setZValue(value.toBool() ? 10000.0 : static_cast<qreal>(this->z_depth));
  }

  return QGraphicsRectItem::itemChange(change, value);
}

void FrameItem::set_is_hovered(bool hovered)
{
  this->is_hovered = hovered;
  this->setZValue(
      hovered ? 10001.0
              : (this->isSelected() ? 10000.0 : static_cast<qreal>(this->z_depth)));
  this->update();
}

void FrameItem::set_locked(bool new_locked)
{
  this->locked = new_locked;
  this->setFlag(QGraphicsItem::ItemIsMovable, !new_locked);
  this->setFlag(QGraphicsItem::ItemIsSelectable, !new_locked);
  this->setAcceptedMouseButtons(new_locked ? Qt::NoButton : Qt::AllButtons);
  this->setAcceptHoverEvents(!new_locked);
  if (new_locked)
    this->setSelected(false);
  this->update();
}

void FrameItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
  const QPointF pos = event->pos();
  const bool    left_button = event->buttons() & Qt::LeftButton;

  if (this->is_rotated && left_button)
  {
    const QPointF pivot(0.5 * COORD_SCALE * this->size.x(),
                        0.5 * COORD_SCALE * this->size.y());
    const QPointF top_mid(0.5 * COORD_SCALE * this->size.x(),
                          COORD_SCALE * this->size.y());

    float pos_angle = std::atan2(pos.y() - pivot.y(), pos.x() - pivot.x());
    float ref_angle = std::atan2(top_mid.y() - pivot.y(), top_mid.x() - pivot.x());
    float delta = (pos_angle - ref_angle) * hmap::cte::rad_to_deg;

    float new_angle = this->angle + delta;
    if (this->snap_enabled && !(event->modifiers() & Qt::AltModifier))
      new_angle = std::round(new_angle / 5.f) * 5.f;

    this->set_angle(new_angle);
  }
  else if (this->is_resized && left_button)
  {
    QPointF new_size = pos / COORD_SCALE;

    // 1:1 aspect ratio scaling if Shift
    if (event->modifiers() & Qt::ShiftModifier)
    {
      const qreal dmax = std::max(new_size.x(), new_size.y());
      new_size = QPointF(dmax, dmax);
    }

    // prevent negative sizes and snap to 0.25 unless Alt is held
    if (this->snap_enabled && !(event->modifiers() & Qt::AltModifier))
    {
      new_size.setX(std::round(new_size.x() / 0.25) * 0.25);
      new_size.setY(std::round(new_size.y() / 0.25) * 0.25);
    }
    new_size = QPointF(std::max(0.1, new_size.x()), std::max(0.1, new_size.y()));

    this->set_size(new_size);
  }

  QGraphicsRectItem::mouseMoveEvent(event);
}

QRectF FrameItem::boundingRect() const
{
  return this->rect().united(this->rotation_rect).adjusted(-4.0, -4.0, 4.0, 4.0);
}

QPainterPath FrameItem::shape() const
{
  QPainterPath path;
  path.addRect(this->rect());
  path.addRect(this->rotation_rect);
  return path;
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

  const qreal lod = std::max(
      1e-4,
      QStyleOptionGraphicsItem::levelOfDetailFromTransform(painter->worldTransform()));
  const qreal inv_lod = 1.0 / lod;

  // contour
  painter->setBrush(Qt::NoBrush);
  painter->setPen(QPen(is_hovered ? QColor(220, 220, 220, 255) : color_border,
                       (is_hovered ? 2.0 : 1.5) * inv_lod));
  painter->drawRect(base_rect);

  // screen-invariant handle geometry
  const qreal   h_px = 12.0 * inv_lod;
  const qreal   stem_px = 22.0 * inv_lod;
  const QPointF top_mid(0.5 * base_rect.width(), base_rect.height());
  const QPointF rot_center(top_mid.x(), top_mid.y() + stem_px);

  this->rotation_rect = QRectF(rot_center.x() - 0.5 * h_px,
                               rot_center.y() - 0.5 * h_px,
                               h_px,
                               h_px);
  this->resize_rect = QRectF(base_rect.width() - h_px,
                             base_rect.height() - h_px,
                             h_px,
                             h_px);

  // rotation stem + circular knob
  painter->setPen(QPen(is_rotated ? color_hovered : color_border, 1.5 * inv_lod));
  painter->drawLine(top_mid, rot_center);
  painter->setBrush(is_rotated ? color_hovered : QColor(80, 160, 255, 220));
  painter->drawEllipse(this->rotation_rect);

  // resize corner grip
  painter->setPen(QPen(is_resized ? color_hovered : color_border, 1.5 * inv_lod));
  painter->setBrush(is_resized ? color_hovered : QColor(255, 170, 60, 220));
  painter->drawRect(this->resize_rect);

  // screen-invariant label
  painter->save();
  painter->translate(4.0 * inv_lod, base_rect.height() - 4.0 * inv_lod);
  painter->scale(inv_lod, -inv_lod);
  painter->setPen(color_hovered);
  painter->drawText(QPointF(0.0, 12.0), QString::fromStdString(this->id));
  painter->restore();
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
