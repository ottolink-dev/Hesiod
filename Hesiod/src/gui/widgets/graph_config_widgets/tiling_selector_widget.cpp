/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QGridLayout>

#include "hesiod/gui/widgets/graph_config_widgets/tiling_selector_widget.hpp"

namespace hesiod
{

TilingSelectorWidget::TilingSelectorWidget(const glm::ivec2 &tiling, QWidget *parent)
    : QWidget(parent), current_tiling(tiling)
{
  QGridLayout *layout = new QGridLayout(this);

  layout->addWidget(new QLabel("Tiling"), 0, 0);

  this->slider_tiling = new QSlider(Qt::Horizontal);
  this->slider_tiling->setRange(1, 4); // 2 -> 16
  this->slider_tiling->setSingleStep(1);
  this->slider_tiling->setPageStep(1);
  this->slider_tiling->setValue(static_cast<int>(std::log2(tiling.x)));

  layout->addWidget(this->slider_tiling, 0, 1);

  this->label_tiling = new QLabel(QString("%1x%2").arg(tiling.x).arg(tiling.y));

  layout->addWidget(this->label_tiling, 0, 2);

  this->slider_tiling->setToolTip("Adjust tiling (2^n)");

  connect(this->slider_tiling,
          &QSlider::valueChanged,
          this,
          [this]() { this->update_tiling(); });

  this->update_tiling();
}

glm::ivec2 TilingSelectorWidget::tiling() const { return this->current_tiling; }

void TilingSelectorWidget::update_tiling()
{
  int exp = this->slider_tiling->value();
  int val = 1 << exp;

  this->current_tiling = glm::ivec2(val, val);

  this->label_tiling->setText(
      QString("%1x%2").arg(this->current_tiling.x).arg(this->current_tiling.y));

  emit this->tiling_changed(this->current_tiling);
}

} // namespace hesiod
