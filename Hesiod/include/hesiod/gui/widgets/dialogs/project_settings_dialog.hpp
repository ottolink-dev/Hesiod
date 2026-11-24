/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QDialog>

#include "hesiod/model/project_model.hpp"

namespace hesiod
{

class ProjectSettingsDialog : public QDialog
{
  Q_OBJECT

public:
  ProjectSettingsDialog(ProjectModel *p_project_model, QWidget *parent = nullptr);

private:
  ProjectModel *p_project_model;
};

} // namespace hesiod