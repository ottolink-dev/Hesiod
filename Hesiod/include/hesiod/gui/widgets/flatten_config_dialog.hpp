/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QSlider>
#include <QWidget>

#include "hesiod/model/graph/flatten_config.hpp"

namespace hesiod
{

// =====================================
// FlattenConfigDialog
// =====================================
class FlattenConfigDialog : public QDialog
{
  Q_OBJECT

public:
  FlattenConfigDialog() = default;
  FlattenConfigDialog(FlattenConfig &export_param, QWidget *parent = nullptr);

private:
  FlattenConfig &export_param;

  // Recompute export_param.shape from the aspect combo + resolution slider(s),
  // keeping the short edge a multiple of SHAPE_SNAP so it stays tiling-friendly.
  void recompute_shape();

  QComboBox *combo_aspect;

  QSlider *slider_shape; // long edge (2^n)
  QLabel  *label_shape;  // shows the computed "W x H"

  QLabel  *label_shape_y_text; // "height (custom)" row, shown only for Custom
  QSlider *slider_shape_y;     // short edge (2^n), Custom only
  QLabel  *label_shape_y;

  QSlider *slider_tiling;
  QLabel  *label_tiling;

  QSlider *slider_overlap;
  QLabel  *label_overlap;
  float    vmin = 0.f;
  float    vmax = 0.75f;
  int      steps = 3;
};

} // namespace hesiod