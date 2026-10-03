/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <memory>

#include <QContextMenuEvent>
#include <QLabel>
#include <QMouseEvent>

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

enum PreviewType : int
{
  GRAYSCALE,
  MAGMA,
  TERRAIN,
  SLOPE_ELEVATION_HEATMAP,
  HISTOGRAM,
};

static std::map<std::string, PreviewType> preview_type_map = {
    {"cmap Grayscale", PreviewType::GRAYSCALE},
    {"cmap Magma", PreviewType::MAGMA},
    {"cmap Terrain (hillshade)", PreviewType::TERRAIN},
    {"Histogram view", PreviewType::HISTOGRAM},
    // {"Slope/elev. heatmap", PreviewType::SLOPE_ELEVATION_HEATMAP},
};

// =====================================
// DataPreview
// =====================================
class DataPreview : public QLabel
{
public:
  DataPreview() = default;
  DataPreview(std::weak_ptr<BaseNode> model, QWidget *parent = nullptr);

  const QPixmap &get_preview_pixmap() const;

public slots:
  void clear_preview();
  void update_preview();

protected:
  void contextMenuEvent(QContextMenuEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;

private:
  std::weak_ptr<BaseNode> model;
  int                     preview_port_index;
  PreviewType             preview_type = PreviewType::GRAYSCALE;
  QPixmap                 preview_pixmap;
};

} // namespace hesiod