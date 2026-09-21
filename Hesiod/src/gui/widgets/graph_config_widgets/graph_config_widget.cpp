/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QPushButton>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/graph_config_widgets/graph_config_dialog.hpp"
#include "hesiod/gui/widgets/graph_config_widgets/overlap_selector_widget.hpp"
#include "hesiod/gui/widgets/graph_config_widgets/shape_selector_widget.hpp"
#include "hesiod/gui/widgets/graph_config_widgets/tiling_selector_widget.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/logger.hpp"

namespace hesiod
{

GraphConfigDialog::GraphConfigDialog(GraphConfig &config, QWidget *parent)
    : QDialog(parent), config(config)
{
  this->setWindowTitle("Hesiod - Model configuration");

  QGridLayout *layout = new QGridLayout(this);

  int row = 0;

  // --- helper for section title labels

  auto add_section_title = [this, layout, &row](const std::string &title_text)
  {
    QLabel     *label = new QLabel(title_text.c_str(), this);
    std::string style = std::format(
        "font-weight: bold; color: {};",
        HSD_CTX.app_settings.colors.text_primary.name().toStdString());
    label->setStyleSheet(style.c_str());
    layout->addWidget(label, row, 0, 1, 3);
    row++;
  };

  // --- domain shape

  add_section_title("Domain Shape");

  QLabel *label_shape_note = new QLabel(
      "Defines the spatial resolution and aspect ratio, directly impacting the "
      "discretization and detail level of the current graph.",
      this);

  std::string shape_note_style = std::format(
      "color: {};",
      HSD_CTX.app_settings.colors.text_secondary.name().toStdString());

  label_shape_note->setStyleSheet(shape_note_style.c_str());
  label_shape_note->setWordWrap(true);
  resize_font(label_shape_note, -1);

  layout->addWidget(label_shape_note, row, 0, 1, 4);

  row++;

  auto *shape_selector = new ShapeSelectorWidget(this->config.shape);

  layout->addWidget(shape_selector, row, 0, 3, 3);

  connect(shape_selector,
          &ShapeSelectorWidget::shape_changed,
          [this](const glm::ivec2 &shape) { this->config.shape = shape; });

  row += 3;

  // --- advanced parameters

  add_section_title("Advanced Parameters");

  QLabel *label_advanced_note = new QLabel(
      "Advanced parameters configure the computation setup (distribution across "
      "CPUs and GPU device) and do not impact the final result.",
      this);

  std::string note_style = std::format(
      "color: {};",
      HSD_CTX.app_settings.colors.text_secondary.name().toStdString());

  label_advanced_note->setStyleSheet(note_style.c_str());
  label_advanced_note->setWordWrap(true);
  resize_font(label_advanced_note, -1);

  layout->addWidget(label_advanced_note, row, 0, 1, 4);

  row++;

  // --- tiling

  auto *tiling_selector = new TilingSelectorWidget(this->config.tiling);

  layout->addWidget(tiling_selector, row, 0, 1, 3);

  connect(tiling_selector,
          &TilingSelectorWidget::tiling_changed,
          [this](const glm::ivec2 &tiling) { this->config.tiling = tiling; });

  row++;

  // --- overlap

  auto *overlap_selector = new OverlapSelectorWidget(this->config.overlap);

  layout->addWidget(overlap_selector, row, 0, 1, 3);

  connect(overlap_selector,
          &OverlapSelectorWidget::overlap_changed,
          [this](float overlap) { this->config.overlap = overlap; });

  row++;

  // --- memory

  QLabel *label_memory_text = new QLabel("Cache Data on Disk", this);
  layout->addWidget(label_memory_text, row, 0);

  this->checkbox_memory = new QCheckBox(this);
  this->checkbox_memory->setChecked(this->config.cm_cpu.trim_storage);
  layout->addWidget(this->checkbox_memory, row, 1);

  this->checkbox_memory->setToolTip(
      "Reduce memory footprint by caching node data on disk");

  this->connect(this->checkbox_memory,
                &QCheckBox::toggled,
                [this](bool checked)
                {
                  this->config.cm_cpu.trim_storage = checked;
                  this->config.cm_gpu.trim_storage = checked;
                  this->config.cm_single_array.trim_storage = checked;
                });

  row++;

  // --- compute modes

  auto add_compute_combobox =
      [this, layout, &row](const std::string &label_text, hmap::ForEachMode &mode)
  {
    QLabel *label = new QLabel(label_text.c_str(), this);
    layout->addWidget(label, row, 0);

    QComboBox *combobox = new QComboBox(this);
    for (auto &[name, id] : hmap::for_each_mode_as_string)
    {
      combobox->addItem(QString::fromStdString(name));
      if (id == static_cast<int>(mode))
        combobox->setCurrentText(QString::fromStdString(name));
    }

    this->connect(combobox,
                  QOverload<int>::of(&QComboBox::currentIndexChanged),
                  [this, combobox, &mode]()
                  {
                    std::string current_choice = combobox->currentText().toStdString();
                    Logger::log()->trace("Selected compute mode: {}", current_choice);
                    mode = static_cast<hmap::ForEachMode>(
                        hmap::for_each_mode_as_string.at(current_choice));
                  });

    layout->addWidget(combobox, row, 1, 1, 3);
    row++;
  };

  add_compute_combobox("CPU", this->config.cm_cpu.mode);
  add_compute_combobox("GPU", this->config.cm_gpu.mode);

  // --- buttons

  QDialogButtonBox *button_box = new QDialogButtonBox(QDialogButtonBox::Ok |
                                                          QDialogButtonBox::Cancel,
                                                      this);
  button_box->button(QDialogButtonBox::Ok)->setDefault(true);
  layout->addWidget(button_box, row, 0, 1, 3);

  this->connect(button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  this->connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  this->setLayout(layout);
}

} // namespace hesiod
