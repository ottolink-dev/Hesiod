/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once

#include <QLabel>
#include <QSlider>
#include <QWidget>

#include <glm/vec2.hpp>

namespace hesiod
{

class TilingSelectorWidget : public QWidget
{
  Q_OBJECT

public:
  explicit TilingSelectorWidget(const glm::ivec2 &tiling, QWidget *parent = nullptr);

  glm::ivec2 tiling() const;
  void       update_tiling();

signals:
  void tiling_changed(const glm::ivec2 &tiling);

private:
  glm::ivec2 current_tiling;

  QSlider *slider_tiling{nullptr};
  QLabel  *label_tiling{nullptr};
};

} // namespace hesiod