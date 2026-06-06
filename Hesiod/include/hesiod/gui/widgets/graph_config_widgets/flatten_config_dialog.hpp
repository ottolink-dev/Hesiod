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
};

} // namespace hesiod