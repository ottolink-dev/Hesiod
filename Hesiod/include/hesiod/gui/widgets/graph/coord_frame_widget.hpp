/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <memory>

#include <QGraphicsRectItem>
#include <QGraphicsView>
#include <QPixmap>

#define COORD_SCALE 1e2f

namespace hesiod
{

// forward
class GraphManager;

// =====================================
// FrameItem
// =====================================
class FrameItem : public QObject, public QGraphicsRectItem
{
  Q_OBJECT

public:
  explicit FrameItem(const std::string &id, QPointF origin, QPointF size, float angle);

  // --- Qt paint override ---
  void paint(QPainter                       *painter,
             const QStyleOptionGraphicsItem *option,
             QWidget                        *widget = nullptr) override;

  // --- Accessors ---
  std::string get_id() const { return id; }
  void        get_frame_parameter(QPointF &origin_out,
                                  QPointF &size_out,
                                  float   &angle_out) const;

  // --- Geometry setters ---
  void set_origin(QPointF new_origin);
  void set_size(QPointF new_size);
  void set_angle(float new_angle);
  void set_geometry(QPointF new_origin, QPointF new_size, float new_angle);
  void set_z_depth(int new_z_depth);

  // --- Appearance ---
  void set_background_image(QImage new_image);

  // --- Interaction state setters ---
  void set_is_resized(bool resized);
  void set_is_rotated(bool rotated);

  // --- Layout/Geometry update ---
  void update_item_geometry();

signals:
  void has_changed(const std::string &id);

protected:
  // --- Interaction overrides ---
  void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
  void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
  void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;

  // --- Qt item change notification ---
  QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
  void update_cursor_and_flags();

  // --- Identity ---
  std::string id = "UNDEFINED";

  // --- Geometry ---
  QPointF origin;
  QPointF size;
  float   angle = 0.0f;
  int     z_depth = 0;

  // --- Visuals ---
  QPixmap pixmap;

  // --- Interaction state ---
  bool is_hovered = false;
  bool is_rotated = false;
  bool is_resized = false;

  // --- Handles ---
  QRectF rotation_rect;
  QRectF resize_rect;
  float  handle_size = 0.2f * COORD_SCALE;
};

// =====================================
// CoordFrameWidget
// =====================================
class CoordFrameWidget : public QGraphicsView
{
  Q_OBJECT

public:
  explicit CoordFrameWidget(std::weak_ptr<GraphManager> p_graph_manager,
                            QWidget                    *parent = nullptr);

  void clear();

  // --- Frame management ---
  void       add_frame(const std::string &id);
  FrameItem *get_frame_ref(const std::string &id) { return frames.at(id); }
  void       remove_frame(const std::string &id);

public slots:
  void on_zoom_to_content();
  void update_frames_z_depth();
  void reset();

signals:
  void has_changed();

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;

private:
  std::weak_ptr<GraphManager>        p_graph_manager;
  std::map<std::string, FrameItem *> frames;
};

} // namespace hesiod