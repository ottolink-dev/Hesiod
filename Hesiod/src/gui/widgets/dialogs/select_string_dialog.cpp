/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "hesiod/gui/widgets/dialogs/select_string_dialog.hpp"
#include "hesiod/logger.hpp"

namespace hesiod
{

SelectStringDialog::SelectStringDialog(const std::vector<std::string> &options,
                                       const std::string              &message,
                                       QWidget                        *parent)
    : QDialog(parent)
{
  this->setWindowTitle("Select a Value");

  QVBoxLayout *main_layout = new QVBoxLayout(this);

  QLabel *label = new QLabel(message.c_str(), this);
  label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  main_layout->addWidget(label);

  this->combo_box = new QComboBox(this);
  this->combo_box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  for (const auto &s : options)
    this->combo_box->addItem(QString::fromStdString(s));

  main_layout->addWidget(this->combo_box);

  QHBoxLayout *button_layout = new QHBoxLayout();
  button_layout->addStretch();

  QPushButton *ok_button = new QPushButton("OK", this);
  QPushButton *cancel_button = new QPushButton("Cancel", this);

  button_layout->addWidget(ok_button);
  button_layout->addWidget(cancel_button);

  main_layout->addLayout(button_layout);

  // Connections
  this->connect(ok_button, &QPushButton::clicked, this, &QDialog::accept);
  this->connect(cancel_button, &QPushButton::clicked, this, &QDialog::reject);

  // Optional: compact and fixed size hint
  this->setMinimumWidth(250);
  this->setMaximumWidth(500);
}

std::string SelectStringDialog::selected_value() const
{
  if (!this->combo_box)
  {
    Logger::log()->error("SelectStringDialog::selected_value: combo_box is nullptr");
    return {};
  }

  return this->combo_box->currentText().toStdString();
}

} // namespace hesiod
