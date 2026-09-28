/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <vector>

#include <QString>

#include "hesiod/gui/widgets/message_dialog.hpp"
#include "hesiod/model/error/error.hpp"

class QVBoxLayout;

namespace hesiod
{

// =====================================
// ErrorDialog
// =====================================

// A list of problems (e.g. nodes or links that could not be restored while
// loading a project), in the application's message-dialog chrome. Each problem
// is a readable row -- what failed, then why, tagged with its category --
// rather than one long monospace line; "Copy details" puts the raw text on the
// clipboard for bug reports.
//
// exec() returns Accepted for Continue / OK and Rejected for Cancel.
class ErrorDialog : public MessageDialog
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
  struct Item
  {
    QString tag, text;
  };

  void setup_ui(const std::vector<Item> &items, bool show_cancel_button);

  QString raw_text;
};

} // namespace hesiod
