/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QDesktopServices>
#include <QLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QUrl>
#include <QWidget>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/dialogs/documentation_popup.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"

namespace hesiod
{

DocumentationPopup::DocumentationPopup(const std::string &title,
                                       const std::string &html_source,
                                       QWidget           *parent)
    : QWidget(parent)
{
  this->setWindowTitle(title.c_str());
  this->setMinimumSize(512, 768);

  QVBoxLayout *layout = new QVBoxLayout(this);

  // Create a QTextBrowser
  QTextBrowser *text_browser = new QTextBrowser(this);

  std::string color = HSD_CTX.app_settings.colors.accent.name().toStdString();
  std::string style = std::format(
      "a {{ color: {}; font-weight: bold; text-decoration: none; }}"
      "a:visited {{ color: {}; }}",
      color,
      color);

  text_browser->document()->setDefaultStyleSheet(style.c_str());

  text_browser->setHtml(html_source.c_str());
  text_browser->setOpenExternalLinks(true); // handled manually

  QPushButton *close_button = new QPushButton("Close", this);

  layout->addWidget(text_browser);
  layout->addWidget(close_button);
  this->setLayout(layout);

  this->connect(close_button, &QPushButton::clicked, this, &QWidget::close);
}

} // namespace hesiod
