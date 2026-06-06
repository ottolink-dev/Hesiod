/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QCheckBox>
#include <QDialog>
#include <QLabel>
#include <QSlider>
#include <QWidget>

#include "hesiod/model/graph/graph_config.hpp"

namespace hesiod
{

// =====================================
// GraphConfigDialog
// =====================================
class GraphConfigDialog : public QDialog
{
  Q_OBJECT

public:
  GraphConfigDialog() = default;
  GraphConfigDialog(GraphConfig &config, QWidget *parent = nullptr);

private:
  GraphConfig &config;

  QSlider *slider_shape;
  QLabel  *label_shape;

  QSlider *slider_tiling;
  QLabel  *label_tiling;

  QSlider *slider_overlap;
  QLabel  *label_overlap;
  float    vmin = 0.f;
  float    vmax = 0.75f;
  int      steps = 3;

  QCheckBox *checkbox_memory;
};

} // namespace hesiod