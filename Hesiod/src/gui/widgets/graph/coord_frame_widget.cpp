/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QGraphicsRectItem>
#include <QMouseEvent>

#include "hesiod/gui/widgets/graph/coord_frame_widget.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/graph/graph_manager.hpp"
#include "hesiod/model/graph/graph_node.hpp"

#define MAX_SIZE 40000

namespace hesiod
{

CoordFrameWidget::CoordFrameWidget(std::weak_ptr<GraphManager> p_graph_manager,
                                   QWidget                    *parent)
    : QGraphicsView(parent), p_graph_manager(p_graph_manager)
{
  Logger::log()->trace("CoordFrameWidget::CoordFrameWidget");

  auto gm = this->p_graph_manager.lock();
  if (!gm)
    return;

  // build layout
  this->setRenderHint(QPainter::Antialiasing);
  this->setRenderHint(QPainter::SmoothPixmapTransform);
  this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  this->setDragMode(QGraphicsView::NoDrag);
  this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  this->setStyleSheet("QGraphicsView { border: 1px solid #5B5B5B; border-radius: 4px;}");

  // flip Y-axis
  QTransform transform;
  transform.scale(1.f, -1.f);
  this->setTransform(transform);

  this->setScene(new QGraphicsScene());
  this->scene()->setSceneRect(-MAX_SIZE, -MAX_SIZE, (MAX_SIZE * 2), (MAX_SIZE * 2));
  // this->setBackgroundBrush(QBrush(GN_STYLE->viewer.color_bg));

  // populate scene
  this->reset();
}

void CoordFrameWidget::add_frame(const std::string &id)
{
  auto gm = this->p_graph_manager.lock();
  if (!gm)
    return;

  GraphNodeMap graphs = gm->get_graph_nodes();

  auto it = graphs.find(id);
  if (it == graphs.end())
  {
    Logger::log()->error("CoordFrameWidget::add_frame:: Graph ID not found: {}", id);
    return;
  }

  glm::vec2 origin = graphs.at(id)->get_origin();
  glm::vec2 size = graphs.at(id)->get_size();
  float     angle = graphs.at(id)->get_rotation_angle();

  FrameItem *new_frame = new FrameItem(id,
                                       QPointF(origin.x, origin.y),
                                       QPointF(size.x, size.y),
                                       angle);
  this->scene()->addItem(new_frame);
  new_frame->set_z_depth(gm->get_graph_order_index(id));

  // keep track of frame item ptr
  this->frames[id] = new_frame;

  // connections
  this->connect(new_frame,
                &FrameItem::has_changed,
                this,
                [this](const std::string & /* id */) { Q_EMIT this->has_changed(); });
}

void CoordFrameWidget::clear()
{
  std::vector<std::string> id_list = {};

  for (auto &[id, _] : this->frames)
    id_list.push_back(id);

  for (auto &id : id_list)
    this->remove_frame(id);
}

void CoordFrameWidget::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton)
    this->setDragMode(QGraphicsView::ScrollHandDrag);

  QGraphicsView::mousePressEvent(event);
}

void CoordFrameWidget::mouseReleaseEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton)
    this->setDragMode(QGraphicsView::NoDrag);

  QGraphicsView::mouseReleaseEvent(event);
}

void CoordFrameWidget::on_zoom_to_content()
{
  QRectF bbox = this->scene()->itemsBoundingRect();

  // add a margin
  float margin_x = 0.1f * bbox.width();
  float margin_y = 0.1f * bbox.height();
  bbox.adjust(-margin_x, -margin_y, margin_x, margin_y);

  this->fitInView(bbox, Qt::KeepAspectRatio);
}

void CoordFrameWidget::remove_frame(const std::string &id)
{
  auto it = frames.find(id);
  if (it == frames.end())
    return;

  FrameItem *item = it->second;

  this->scene()->removeItem(item); // removes AND deletes because scene owns it
  delete item;                     // explicitly delete for clarity and early free (safe)

  frames.erase(it);

  this->update();
}

void CoordFrameWidget::reset()
{
  Logger::log()->trace("CoordFrameWidget::reset");

  auto gm = this->p_graph_manager.lock();
  if (!gm)
    return;

  // store keys because the map is altered by remove_frame
  std::vector<std::string> keys;
  keys.reserve(frames.size());

  for (auto &[id, _] : this->frames)
    keys.push_back(id);

  for (auto &id : keys)
    this->remove_frame(id);

  // re-populate scene
  for (auto &id : gm->get_graph_order())
  {
    Logger::log()->trace("CoordFrameWidget::CoordFrameWidget: adding graph: {}", id);
    this->add_frame(id);
  }
}

void CoordFrameWidget::update_frames_z_depth()
{

  auto gm = this->p_graph_manager.lock();
  if (!gm)
    return;

  for (size_t k = 0; k < gm->get_graph_order().size(); ++k)
  {
    const std::string id = gm->get_graph_order()[k];
    auto              it = frames.find(id);
    if (it != frames.end())
      it->second->set_z_depth((int)k);
  }
}

void CoordFrameWidget::wheelEvent(QWheelEvent *event)
{
  const float factor = 1.2f;
  QPointF     mouse_scene_pos = this->mapToScene(event->position().toPoint());

  if (event->angleDelta().y() > 0)
    this->scale(factor, factor);
  else
    this->scale(1.f / factor, 1.f / factor);

  // adjust the view to maintain the zoom centered on the mouse position
  QPointF new_mouse_scene_pos = this->mapToScene(event->position().toPoint());
  QPointF delta = new_mouse_scene_pos - mouse_scene_pos;
  this->translate(delta.x(), delta.y());

  event->accept();
}

} // namespace hesiod
