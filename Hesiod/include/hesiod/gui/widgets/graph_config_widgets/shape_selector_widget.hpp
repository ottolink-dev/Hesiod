/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QComboBox>
#include <QLabel>
#include <QSlider>
#include <QWidget>

#include <glm/vec2.hpp>

#include "hesiod/gui/widgets/graph_config_widgets/shape_selector_widget.hpp"

namespace hesiod
{

class ShapeSelectorWidget : public QWidget
{
  Q_OBJECT

public:
  explicit ShapeSelectorWidget(const glm::ivec2 &shape,
                               int               min_power_of_two_exp = 8,
                               int               max_power_of_two_exp = 12,
                               QWidget          *parent = nullptr);

  // Recompute export_param.shape from the aspect combo + resolution slider(s),
  // keeping the short edge a multiple of SHAPE_SNAP so it stays tiling-friendly.
  void recompute_shape();

  glm::ivec2 shape() const;

signals:
  void shape_changed(const glm::ivec2 &shape);

private:
  glm::ivec2 current_shape;

  QComboBox *combo_aspect{nullptr};

  QSlider *slider_shape{nullptr};   // long edge (2^n)
  QSlider *slider_shape_y{nullptr}; // short edge (2^n), Custom only

  QLabel *label_shape{nullptr};        // shows the computed "W x H"
  QLabel *label_shape_y_text{nullptr}; // "height (custom)" row, shown only for Custom
  QLabel *label_shape_y{nullptr};
};

} // namespace hesiod