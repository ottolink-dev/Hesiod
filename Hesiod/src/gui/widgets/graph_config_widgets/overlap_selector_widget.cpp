/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QGridLayout>

#include "hesiod/gui/widgets/graph_config_widgets/overlap_selector_widget.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"

namespace hesiod
{

OverlapSelectorWidget::OverlapSelectorWidget(float overlap, QWidget *parent)
    : QWidget(parent), current_overlap(overlap)
{
  QGridLayout *layout = new QGridLayout(this);

  layout->addWidget(new QLabel("Overlap"), 0, 0);

  this->slider_overlap = new QSlider(Qt::Horizontal);
  this->slider_overlap->setRange(0, this->steps);
  this->slider_overlap->setSingleStep(1);
  this->slider_overlap->setPageStep(1);

  int pos = float_to_slider_pos(overlap, this->vmin, this->vmax, this->steps);

  this->slider_overlap->setValue(pos);

  layout->addWidget(this->slider_overlap, 0, 1);

  this->label_overlap = new QLabel(QString::number(overlap, 'f', 2));

  layout->addWidget(this->label_overlap, 0, 2);

  this->slider_overlap->setToolTip("Adjust overlap fraction");

  connect(this->slider_overlap,
          &QSlider::valueChanged,
          this,
          [this]() { this->update_overlap(); });

  this->update_overlap();
}

float OverlapSelectorWidget::overlap() const { return this->current_overlap; }

void OverlapSelectorWidget::update_overlap()
{
  this->current_overlap = slider_pos_to_float(this->slider_overlap->value(),
                                              this->vmin,
                                              this->vmax,
                                              this->steps);

  this->label_overlap->setText(QString::number(this->current_overlap, 'f', 2));

  Q_EMIT this->overlap_changed(this->current_overlap);
}

} // namespace hesiod
