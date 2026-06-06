/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QPoint>
#include <QPushButton>

#include "hesiod/gui/widgets/graph_config_widgets/flatten_config_dialog.hpp"
#include "hesiod/gui/widgets/graph_config_widgets/overlap_selector_widget.hpp"
#include "hesiod/gui/widgets/graph_config_widgets/shape_selector_widget.hpp"
#include "hesiod/gui/widgets/graph_config_widgets/tiling_selector_widget.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/logger.hpp"

namespace hesiod
{

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

  // --- shape

  auto *shape_selector = new ShapeSelectorWidget(this->export_param.shape);

  layout->addWidget(shape_selector, row, 0, 3, 3);

  connect(shape_selector,
          &ShapeSelectorWidget::shape_changed,
          [this](const glm::ivec2 &shape) { this->export_param.shape = shape; });

  row += 3;

  // --- tiling

  auto *tiling_selector = new TilingSelectorWidget(this->export_param.tiling);

  layout->addWidget(tiling_selector, row, 0, 1, 3);

  connect(tiling_selector,
          &TilingSelectorWidget::tiling_changed,
          [this](const glm::ivec2 &tiling) { this->export_param.tiling = tiling; });

  row++;

  // --- overlap

  auto *overlap_selector = new OverlapSelectorWidget(this->export_param.overlap);

  layout->addWidget(overlap_selector, row, 0, 1, 3);

  connect(overlap_selector,
          &OverlapSelectorWidget::overlap_changed,
          [this](float overlap) { this->export_param.overlap = overlap; });

  row++;

  // --- buttons

  QDialogButtonBox *button_box = new QDialogButtonBox(QDialogButtonBox::Ok |
                                                      QDialogButtonBox::Cancel);

  layout->addWidget(button_box, row, 0, 1, 3);

  this->setLayout(layout);

  connect(button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

} // namespace hesiod
