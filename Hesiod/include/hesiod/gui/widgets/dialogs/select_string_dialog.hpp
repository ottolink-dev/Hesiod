/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <string>

#include <QDialog>
#include <QString>
#include <QStringList>

class QComboBox;
class QPushButton;

namespace hesiod
{

class SelectStringDialog : public QDialog
{
  Q_OBJECT

public:
  explicit SelectStringDialog(const std::vector<std::string> &options,
                              const std::string              &message = "",
                              QWidget                        *parent = nullptr);

  std::string selected_value() const;

private:
  QComboBox *combo_box;
};

} // namespace hesiod