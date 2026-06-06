/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QLabel>
#include <QSlider>
#include <QWidget>

namespace hesiod
{

class OverlapSelectorWidget : public QWidget
{
  Q_OBJECT

public:
  explicit OverlapSelectorWidget(float overlap, QWidget *parent = nullptr);

  float overlap() const;
  void  update_overlap();

signals:
  void overlap_changed(float overlap);

private:
  float current_overlap{0.f};

  QSlider *slider_overlap{nullptr};
  QLabel  *label_overlap{nullptr};

  const float vmin = 0.f;
  const float vmax = 0.75f;
  const int   steps = 3;
};

} // namespace hesiod