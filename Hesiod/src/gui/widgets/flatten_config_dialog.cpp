/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QPoint>
#include <QPushButton>

#include "hesiod/gui/widgets/flatten_config_dialog.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/logger.hpp"

namespace hesiod
{

// short edge is rounded down to a multiple of this so shape / tiling stays clean
#define SHAPE_SNAP 16

FlattenConfigDialog::FlattenConfigDialog(FlattenConfig &export_param, QWidget *parent)
    : QDialog(parent), export_param(export_param)
{
  Logger::log()->trace("FlattenConfigDialog::FlattenConfigDialog");

  this->setWindowTitle("Hesiod - Graph flatten configuration");

  QGridLayout *layout = new QGridLayout(this);

  int row = 0;

  // --- filename

  QLabel *label_fname = new QLabel("filename");
  layout->addWidget(label_fname, row, 0);

  QPushButton *button = new QPushButton(this->export_param.export_path.string().c_str());
  layout->addWidget(button, row, 1, 1, 2);
  row++;

  this->connect(
      button,
      &QPushButton::released,
      [this]()
      {
        std::filesystem::path path = this->export_param.export_path.parent_path();

        QString fname;

        fname = QFileDialog::getSaveFileName(this,
                                             "Select Export File",
                                             path.string().c_str(),
                                             "*.png");

        if (!fname.isNull() && !fname.isEmpty())
          this->export_param.export_path = fname.toStdString();
      });

  // --- aspect ratio (drives the shape; "Custom" unlocks an independent Y)

  QLabel *label_aspect_text = new QLabel("aspect");
  layout->addWidget(label_aspect_text, row, 0);

  this->combo_aspect = new QComboBox();
  // itemData = (width, height) ratio; (0, 0) marks Custom
  this->combo_aspect->addItem("1:1 (square)", QPoint(1, 1));
  this->combo_aspect->addItem("2:1 (equirectangular)", QPoint(2, 1));
  this->combo_aspect->addItem("3:2", QPoint(3, 2));
  this->combo_aspect->addItem("16:9", QPoint(16, 9));
  this->combo_aspect->addItem("Custom", QPoint(0, 0));
  layout->addWidget(this->combo_aspect, row, 1, 1, 2);
  row++;

  // --- shape (long edge, 2^n)

  QLabel *label_shape_text = new QLabel("shape");
  layout->addWidget(label_shape_text, row, 0);

  this->slider_shape = new QSlider(Qt::Horizontal);
  this->slider_shape->setRange(8, 12); // 256 -> 4096
  this->slider_shape->setSingleStep(1);
  this->slider_shape->setPageStep(1);
  int long_edge = std::max(this->export_param.shape.x, this->export_param.shape.y);
  this->slider_shape->setValue((int)std::log2(long_edge));
  layout->addWidget(this->slider_shape, row, 1);

  this->label_shape = new QLabel(QString().asprintf("%dx%d",
                                                    this->export_param.shape.x,
                                                    this->export_param.shape.y));
  layout->addWidget(this->label_shape, row, 2);
  row++;

  // --- short edge (Custom only)

  this->label_shape_y_text = new QLabel("height");
  layout->addWidget(this->label_shape_y_text, row, 0);

  this->slider_shape_y = new QSlider(Qt::Horizontal);
  this->slider_shape_y->setRange(8, 12); // 256 -> 4096
  this->slider_shape_y->setSingleStep(1);
  this->slider_shape_y->setPageStep(1);
  this->slider_shape_y->setValue((int)std::log2(this->export_param.shape.y));
  layout->addWidget(this->slider_shape_y, row, 1);

  this->label_shape_y = new QLabel(QString().asprintf("%d", this->export_param.shape.y));
  layout->addWidget(this->label_shape_y, row, 2);
  row++;

  // pick the combo entry that matches the current shape (else Custom)
  {
    int x = this->export_param.shape.x;
    int y = this->export_param.shape.y;
    int idx = this->combo_aspect->count() - 1; // Custom
    for (int i = 0; i < this->combo_aspect->count(); ++i)
    {
      QPoint r = this->combo_aspect->itemData(i).toPoint();
      if (r.x() > 0 && x * r.y() == y * r.x())
      {
        idx = i;
        break;
      }
    }
    this->combo_aspect->setCurrentIndex(idx);
  }

  connect(this->combo_aspect,
          QOverload<int>::of(&QComboBox::currentIndexChanged),
          [this]() { this->recompute_shape(); });
  connect(this->slider_shape,
          &QSlider::valueChanged,
          [this]() { this->recompute_shape(); });
  connect(this->slider_shape_y,
          &QSlider::valueChanged,
          [this]() { this->recompute_shape(); });

  // initial sync (also sets Custom-row visibility)
  this->recompute_shape();

  // --- tiling

  QLabel *label_tiling_text = new QLabel("tiling");
  layout->addWidget(label_tiling_text, row, 0);

  this->slider_tiling = new QSlider(Qt::Horizontal);
  this->slider_tiling->setRange(1, 8);
  this->slider_tiling->setSingleStep(1);
  this->slider_tiling->setPageStep(1);
  this->slider_tiling->setValue(this->export_param.tiling.x);
  layout->addWidget(this->slider_tiling, row, 1);

  this->label_tiling = new QLabel(QString().asprintf("%dx%d",
                                                     this->export_param.tiling.x,
                                                     this->export_param.tiling.y));
  layout->addWidget(this->label_tiling, row, 2);

  connect(this->slider_tiling,
          &QSlider::valueChanged,
          [this]()
          {
            int pos = this->slider_tiling->value();
            this->export_param.tiling.x = pos;
            this->export_param.tiling.y = pos;
            this->label_tiling->setText(QString().asprintf("%dx%d",
                                                           this->export_param.tiling.x,
                                                           this->export_param.tiling.y));
          });

  row++;

  // --- overlap

  QLabel *label_overlap_text = new QLabel("overlap");
  layout->addWidget(label_overlap_text, row, 0);

  this->slider_overlap = new QSlider(Qt::Horizontal);
  this->slider_overlap->setRange(0, this->steps);
  this->slider_overlap->setSingleStep(1);
  this->slider_overlap->setPageStep(1);
  int pos = float_to_slider_pos(this->export_param.overlap,
                                this->vmin,
                                this->vmax,
                                this->steps);
  this->slider_overlap->setValue(pos);
  layout->addWidget(this->slider_overlap, row, 1);

  this->label_overlap = new QLabel(
      QString().asprintf("%.2f", this->export_param.overlap));
  layout->addWidget(this->label_overlap, row, 2);

  connect(this->slider_overlap,
          &QSlider::valueChanged,
          [this]()
          {
            float v = slider_pos_to_float(this->slider_overlap->value(),
                                          this->vmin,
                                          this->vmax,
                                          this->steps);
            this->label_overlap->setText(QString().asprintf("%.2f", v));
            this->export_param.overlap = v;
          });

  row++;

  // --- buttons

  QDialogButtonBox *button_box = new QDialogButtonBox(QDialogButtonBox::Ok |
                                                      QDialogButtonBox::Cancel);

  layout->addWidget(button_box, row, 0, 1, 3);

  this->setLayout(layout);

  connect(button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void FlattenConfigDialog::recompute_shape()
{
  const QPoint ratio = this->combo_aspect->currentData().toPoint();
  const bool   is_custom = (ratio.x() == 0 || ratio.y() == 0);

  // the height slider is only relevant (and shown) in Custom mode
  this->label_shape_y_text->setVisible(is_custom);
  this->slider_shape_y->setVisible(is_custom);
  this->label_shape_y->setVisible(is_custom);

  int x, y;

  if (is_custom)
  {
    x = 1 << this->slider_shape->value();
    y = 1 << this->slider_shape_y->value();
  }
  else
  {
    const int w = ratio.x();
    const int h = ratio.y();
    const int long_edge = 1 << this->slider_shape->value();

    // long edge follows the slider; short edge is derived from the ratio and
    // snapped down to a multiple of SHAPE_SNAP so shape / tiling stays clean
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

  this->export_param.shape = glm::ivec2(x, y);
  this->label_shape->setText(QString().asprintf("%dx%d", x, y));
  this->label_shape_y->setText(QString().asprintf("%d", y));
}

} // namespace hesiod
