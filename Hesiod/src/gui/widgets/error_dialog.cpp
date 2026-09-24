/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include <QFont>
#include <QFontDatabase>
#include <QPushButton>

#include "hesiod/gui/widgets/error_dialog.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/logger.hpp"

namespace hesiod
{

ErrorDialog::ErrorDialog(const QString            &title,
                         const QString            &message,
                         const std::vector<Error> &errors,
                         bool                      show_cancel_button,
                         QWidget                  *parent)
    : QDialog(parent)
{
  QString error_text;
  for (const auto &err : errors)
  {
    if (!error_text.isEmpty())
      error_text += "\n";
    error_text += "• " + QString::fromStdString(err.get_message());
  }

  this->setWindowTitle(title);
  this->setup_ui(message, error_text, show_cancel_button);
}

ErrorDialog::ErrorDialog(const QString &title,
                         const QString &message,
                         const QString &error_text,
                         bool           show_cancel_button,
                         QWidget       *parent)
    : QDialog(parent)
{
  this->setWindowTitle(title);
  this->setup_ui(message, error_text, show_cancel_button);
}

void ErrorDialog::setup_ui(const QString &message,
                           const QString &error_text,
                           bool           show_cancel_button)
{
  Logger::log()->trace("ErrorDialog::setup_ui");

  this->layout = new QVBoxLayout(this);

  if (!message.isEmpty())
  {
    this->info_label = new QLabel(message, this);
    this->info_label->setWordWrap(true);
    this->layout->addWidget(this->info_label);
  }

  this->error_text_edit = new QPlainTextEdit(this);
  this->error_text_edit->setReadOnly(true);
  this->error_text_edit->setLineWrapMode(QPlainTextEdit::NoWrap);

  // use monospace font for error display
  QFont mono_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  mono_font.setPointSize(9);
  this->error_text_edit->setFont(mono_font);

  this->error_text_edit->setPlainText(error_text);
  this->error_text_edit->setMinimumWidth(560);
  this->error_text_edit->setMinimumHeight(240);
  this->layout->addWidget(this->error_text_edit);

  this->button_box = new QDialogButtonBox(this);
  QPushButton *ok_button = this->button_box->addButton(show_cancel_button ? "Continue"
                                                                          : "OK",
                                                       QDialogButtonBox::AcceptRole);
  ok_button->setDefault(true);

  if (show_cancel_button)
  {
    this->button_box->addButton("Cancel", QDialogButtonBox::RejectRole);
  }

  this->layout->addWidget(this->button_box);

  this->connect(this->button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  this->connect(this->button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

} // namespace hesiod
