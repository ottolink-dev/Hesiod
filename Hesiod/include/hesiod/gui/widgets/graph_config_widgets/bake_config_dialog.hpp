/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General Public
 * License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>

#include "hesiod/model/graph/bake_config.hpp"

namespace hesiod
{

// =====================================
// BakeConfigDialog
// =====================================
class BakeConfigDialog : public QDialog
{
  Q_OBJECT

public:
  explicit BakeConfigDialog(int               max_size,
                            const BakeConfig &initial_value,
                            QWidget          *parent = nullptr);

  BakeConfig get_bake_settings() const;

private:
  QComboBox        *resolution_combo;
  QSlider          *slider;
  QSpinBox         *slider_nvariants;
  QCheckBox        *checkbox_force_distributed;
  QCheckBox        *checkbox_force_auto_export;
  QCheckBox        *checkbox_rename_export_files;
  QCheckBox        *checkbox_force_maximum_fbm_octaves;
  QDialogButtonBox *buttons;
};

} // namespace hesiod
