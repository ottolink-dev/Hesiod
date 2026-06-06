/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cmath>

#include <QGridLayout>
#include <QPoint>

#include "hesiod/gui/widgets/graph_config_widgets/shape_selector_widget.hpp"

namespace hesiod
{

#define SHAPE_SNAP 16

ShapeSelectorWidget::ShapeSelectorWidget(const glm::ivec2 &shape,
                                         int               min_power_of_two_exp,
                                         int               max_power_of_two_exp,
                                         QWidget          *parent)
    : QWidget(parent), current_shape(shape)
{
  QGridLayout *layout = new QGridLayout(this);

  int row = 0;

  // --- aspect ratio

  layout->addWidget(new QLabel("aspect"), row, 0);

  this->combo_aspect = new QComboBox();

  this->combo_aspect->addItem("1:1 (square)", QPoint(1, 1));
  this->combo_aspect->addItem("2:1 (equirectangular)", QPoint(2, 1));
  this->combo_aspect->addItem("3:2", QPoint(3, 2));
  this->combo_aspect->addItem("16:9", QPoint(16, 9));
  this->combo_aspect->addItem("Custom", QPoint(0, 0));

  layout->addWidget(this->combo_aspect, row, 1, 1, 2);

  row++;

  // --- shape (long edge)

  layout->addWidget(new QLabel("shape"), row, 0);

  this->slider_shape = new QSlider(Qt::Horizontal);
  this->slider_shape->setRange(min_power_of_two_exp, max_power_of_two_exp);
  this->slider_shape->setSingleStep(1);
  this->slider_shape->setPageStep(1);

  int long_edge = std::max(shape.x, shape.y);
  this->slider_shape->setValue(static_cast<int>(std::log2(long_edge)));

  layout->addWidget(this->slider_shape, row, 1);

  this->label_shape = new QLabel();
  layout->addWidget(this->label_shape, row, 2);

  row++;

  // --- custom height

  this->label_shape_y_text = new QLabel("height");
  layout->addWidget(this->label_shape_y_text, row, 0);

  this->slider_shape_y = new QSlider(Qt::Horizontal);
  this->slider_shape_y->setRange(min_power_of_two_exp, max_power_of_two_exp);
  this->slider_shape_y->setSingleStep(1);
  this->slider_shape_y->setPageStep(1);
  this->slider_shape_y->setValue(static_cast<int>(std::log2(shape.y)));

  layout->addWidget(this->slider_shape_y, row, 1);

  this->label_shape_y = new QLabel();
  layout->addWidget(this->label_shape_y, row, 2);

  // --- determine aspect preset

  int idx = this->combo_aspect->count() - 1; // Custom

  for (int i = 0; i < this->combo_aspect->count(); ++i)
  {
    QPoint ratio = this->combo_aspect->itemData(i).toPoint();

    if (ratio.x() > 0 && shape.x * ratio.y() == shape.y * ratio.x())
    {
      idx = i;
      break;
    }
  }

  this->combo_aspect->setCurrentIndex(idx);

  connect(this->combo_aspect,
          QOverload<int>::of(&QComboBox::currentIndexChanged),
          this,
          [this]() { this->recompute_shape(); });

  connect(this->slider_shape,
          &QSlider::valueChanged,
          this,
          [this]() { this->recompute_shape(); });

  connect(this->slider_shape_y,
          &QSlider::valueChanged,
          this,
          [this]() { this->recompute_shape(); });

  this->recompute_shape();
}

glm::ivec2 ShapeSelectorWidget::shape() const { return this->current_shape; }

void ShapeSelectorWidget::recompute_shape()
{
  QPoint ratio = this->combo_aspect->currentData().toPoint();

  bool is_custom = ratio.x() == 0 || ratio.y() == 0;

  this->label_shape_y_text->setVisible(is_custom);
  this->slider_shape_y->setVisible(is_custom);
  this->label_shape_y->setVisible(is_custom);

  int x;
  int y;

  if (is_custom)
  {
    x = 1 << this->slider_shape->value();
    y = 1 << this->slider_shape_y->value();
  }
  else
  {
    int w = ratio.x();
    int h = ratio.y();

    int long_edge = 1 << this->slider_shape->value();

    auto snap = [](int v) { return std::max(SHAPE_SNAP, (v / SHAPE_SNAP) * SHAPE_SNAP); };

    if (w >= h)
    {
      x = long_edge;
      y = snap(long_edge * h / w);
    }
    else
    {
      y = long_edge;
      x = snap(long_edge * w / h);
    }
  }

  this->current_shape = glm::ivec2(x, y);

  this->label_shape->setText(QString("%1x%2").arg(x).arg(y));
  this->label_shape_y->setText(QString::number(y));

  Q_EMIT this->shape_changed(this->current_shape);
}

} // namespace hesiod
