/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QDialogButtonBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QVBoxLayout>

#include "hesiod/gui/widgets/dialogs/project_settings_dialog.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"
#include "hesiod/logger.hpp"

namespace hesiod
{

ProjectSettingsDialog::ProjectSettingsDialog(ProjectModel *p_project_model,
                                             QWidget      *parent)
    : QDialog(parent), p_project_model(p_project_model)
{
  Logger::log()->trace("ProjectSettingsDialog::ProjectSettingsDialog");

  if (!this->p_project_model)
    return;
  this->setWindowTitle("Project Settings");
  this->setModal(true);
  this->setAttribute(Qt::WA_DeleteOnClose);
  this->resize(512, 512);

  auto *layout = new QVBoxLayout(this);
  this->setLayout(layout);

  // title
  {
    QLabel *label = new QLabel(this->p_project_model->get_name().c_str());
    label->setAlignment(Qt::AlignCenter);
    resize_font(label, +2);
    layout->addWidget(label);
  }

  // path
  {
    QFont   f("DejaVu Sans Mono", 9);
    QLabel *label = new QLabel(
        QString::fromStdString(this->p_project_model->get_path().string()));
    label->setFont(f);
    layout->addWidget(label);
  }

  // comment
  {
    QLabel *label = new QLabel("Comment");
    layout->addWidget(label);

    QPlainTextEdit *editor = new QPlainTextEdit();
    editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    editor->setPlainText(this->p_project_model->get_comment().c_str());
    editor->setMinimumHeight(64);
    layout->addWidget(editor);

    this->connect(
        editor,
        &QPlainTextEdit::textChanged,
        [this, editor]()
        { this->p_project_model->set_comment(editor->toPlainText().toStdString()); });
  }

  // exit
  {
    QDialogButtonBox *button_box = new QDialogButtonBox(QDialogButtonBox::Ok);
    this->connect(button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    layout->addWidget(button_box);
  }
}

} // namespace hesiod
