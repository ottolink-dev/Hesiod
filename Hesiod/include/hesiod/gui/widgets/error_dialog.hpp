/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QString>
#include <QVBoxLayout>
#include <vector>

#include "hesiod/model/error/error.hpp"

namespace hesiod
{

// =====================================
// ErrorDialog
// =====================================
class ErrorDialog : public QDialog
{
  Q_OBJECT

public:
  ErrorDialog(const QString            &title,
              const QString            &message,
              const std::vector<Error> &errors,
              bool                      show_cancel_button = true,
              QWidget                  *parent = nullptr);

  ErrorDialog(const QString &title,
              const QString &message,
              const QString &error_text,
              bool           show_cancel_button = true,
              QWidget       *parent = nullptr);

private:
  void setup_ui(const QString &message,
                const QString &error_text,
                bool           show_cancel_button);

  // UI elements
  QVBoxLayout      *layout = nullptr;
  QLabel           *info_label = nullptr;
  QPlainTextEdit   *error_text_edit = nullptr;
  QDialogButtonBox *button_box = nullptr;
};

} // namespace hesiod
