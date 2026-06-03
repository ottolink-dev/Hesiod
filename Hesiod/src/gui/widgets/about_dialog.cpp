/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <fstream>

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/about_dialog.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/logger.hpp"

namespace hesiod
{

AboutDialog::AboutDialog()
{
  Logger::log()->trace("AboutDialog::AboutDialog");

  this->resize(512, 512);

  auto *layout = new QVBoxLayout();
  this->setLayout(layout);

  layout->addStretch();

  {
    QLabel *image_label = new QLabel();
    QPixmap pixmap("data/splash_no_bg.png");
    image_label->setPixmap(pixmap);
    image_label->setScaledContents(false);
    image_label->resize(128, 128);
    layout->addWidget(image_label, 0, Qt::AlignHCenter);
  }

  layout->addWidget(new QLabel());
  layout->addWidget(new QLabel());

  {
    QLabel *label = new QLabel(
        "A desktop application for node-based procedural terrain generation.");
    layout->addWidget(label, 0, Qt::AlignHCenter);
  }

  // links with icon
  {
    auto *container = new QWidget();
    auto *link_layout = new QHBoxLayout(container);

    auto add_link = [this, link_layout](const QIcon &icon, const QString &url)
    {
      QPushButton *icon_button = new QPushButton();
      icon_button->setIcon(icon);
      icon_button->setIconSize(QSize(20, 20));
      icon_button->setFlat(true);
      icon_button->setCursor(Qt::PointingHandCursor);
      icon_button->setStyleSheet("QPushButton {"
                                 "  border: none;"
                                 "  background: transparent;"
                                 "}");
      icon_button->setToolTip(url);

      this->connect(icon_button,
                    &QPushButton::clicked,
                    [url]() { QDesktopServices::openUrl(QUrl(url)); });

      link_layout->addWidget(icon_button, 0, Qt::AlignHCenter);
    };

    // links
    add_link(HSD_ICON("home"), "https://hesioddoc.readthedocs.io/en/latest");
    add_link(HSD_ICON("discord"), "https://discord.gg/HBgFCjdV8W");
    add_link(HSD_ICON("github"), "https://github.com/otto-link/Hesiod");
    add_link(HSD_ICON("sourceforge"), "https://sourceforge.net/projects/hesiod");
    add_link(HSD_ICON("youtube"),
             "https://www.youtube.com/playlist?list=PLvla2FXp5tDxbPypf_Mp66gWzX_Lga3DK");

    layout->addWidget(container, 0, Qt::AlignHCenter);
  }

  layout->addWidget(new QLabel());

  // version
  {
    std::ifstream f("data/git_version.txt");
    std::string   version = "???";

    if (f)
    {
      std::stringstream buffer;
      buffer << f.rdbuf();
      version = "Copyright (c) 2023-2025 Otto Link.\n" + buffer.str();
    }

    QLabel *label = new QLabel(version.c_str());
    resize_font(label, -2);
    layout->addWidget(label, 0, Qt::AlignHCenter);
  }

  layout->addWidget(new QLabel());

  // ok button
  {
    QDialogButtonBox *button_box = new QDialogButtonBox(QDialogButtonBox::Ok);

    QPushButton *ok_button = button_box->button(QDialogButtonBox::Ok);
    ok_button->setIcon(HSD_ICON("check"));

    this->connect(button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    layout->addWidget(button_box, 0, Qt::AlignHCenter);
  }

  layout->addStretch();
}

} // namespace hesiod
