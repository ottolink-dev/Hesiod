/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */

/**
 * @file string_input_diaglog.hpp
 * @author  Otto Link (otto.link.bv@gmail.com)
 * @brief
 *
 * @copyright Copyright (c) 2025
 */
#pragma once
#include <QLabel>
#include <QLineEdit>
#include <QSet>

#include "hesiod/gui/widgets/dialogs/message_dialog.hpp"

namespace hesiod
{

// =====================================
// StringInputDialog
// =====================================

// Asks for a name, in the application's dialog chrome. An empty name, or one of
// invalid_strings, is refused with a word under the field.
class StringInputDialog : public MessageDialog
{
public:
  StringInputDialog(const QString                  &title,
                    const QString                  &text,
                    const QString                  &value,
                    const QString                  &accept_label,
                    const std::vector<std::string> &invalid_strings = {},
                    QWidget                        *parent = nullptr);

  QString     get_text() const { return input_line->text().trimmed(); }
  std::string get_text_std_string() const { return this->get_text().toStdString(); }

  void accept() override;

private:
  QLineEdit    *input_line;
  QLabel       *error_label;
  QSet<QString> invalid_strings_qset;
};

} // namespace hesiod
