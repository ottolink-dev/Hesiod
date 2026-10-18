/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QVBoxLayout>

#include "hesiod/gui/widgets/string_input_dialog.hpp"

namespace hesiod
{

StringInputDialog::StringInputDialog(const QString                  &title,
                                     const QString                  &text,
                                     const QString                  &value,
                                     const QString                  &accept_label,
                                     const std::vector<std::string> &invalid_strings,
                                     QWidget                        *parent)
    : MessageDialog(parent, Kind::None, title, text)
{
  this->setWindowTitle("Hesiod - " + title);
  this->set_card_width(400);

  this->input_line = new QLineEdit(value, this);
  this->input_line->selectAll(); // typing replaces the suggestion
  this->body()->addWidget(this->input_line);

  this->error_label = new QLabel(this);
  this->error_label->setObjectName("messageError");
  this->error_label->hide();
  this->body()->addWidget(this->error_label);

  // what was wrong goes away as the user types
  this->connect(this->input_line,
                &QLineEdit::textChanged,
                this->error_label,
                &QLabel::hide);

  for (const auto &s : invalid_strings)
    this->invalid_strings_qset.insert(QString::fromStdString(s));

  this->add_button("Cancel", Role::Secondary, false, true);
  this->add_button(accept_label, Role::Primary, true);
}

void StringInputDialog::accept()
{
  const QString text = this->get_text();

  if (text.isEmpty() || this->invalid_strings_qset.contains(text))
  {
    this->error_label->setText(text.isEmpty() ? "The name can't be empty."
                                              : "This name is already taken.");
    this->error_label->show();
    this->input_line->setFocus();
    this->input_line->selectAll();
    return;
  }

  MessageDialog::accept();
}

} // namespace hesiod
