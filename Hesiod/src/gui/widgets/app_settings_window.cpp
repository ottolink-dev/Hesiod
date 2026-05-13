/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QColorDialog>
#include <QFormLayout>
#include <QMessageBox>
#include <QPushButton>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/app_settings_window.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

AppSettingsWindow::AppSettingsWindow(QWidget *parent) : QWidget(parent)
{
  Logger::log()->trace("AppSettingsWindow::AppSettingsWindow");

  this->setWindowTitle("Hesiod - Application settings");
  this->setup_layout();
}

void AppSettingsWindow::add_description(const std::string &description, int max_length)
{
  if (description.empty())
    return;

  QLabel *label = new QLabel(wrap_text(description, max_length).c_str());

  std::string style = std::format(
      "color: {};",
      HSD_CTX.app_settings.colors.text_secondary.name().toStdString());
  label->setStyleSheet(style.c_str());
  resize_font(label, -1);

  this->layout->addRow(label);
}

void AppSettingsWindow::add_title(const std::string &text, int font_size_delta)
{
  if (text.empty())
    return;

  QLabel *label = new QLabel(text.c_str());

  std::string style = std::format(
      "font-weight: bold; color: {};",
      HSD_CTX.app_settings.colors.text_primary.name().toStdString());
  label->setStyleSheet(style.c_str());
  resize_font(label, font_size_delta);

  this->layout->addRow(label);
}

void AppSettingsWindow::bind_int(const std::string &label, int &value)
{
  auto *spin_box = new QSpinBox();

  // initialize with current value
  spin_box->setValue(value);

  // set a reasonable range...
  spin_box->setRange(1, 64);

  this->connect(spin_box,
                QOverload<int>::of(&QSpinBox::valueChanged),
                this,
                [&value](int v) { value = v; });

  this->layout->addRow(label.c_str(), spin_box);
}

void AppSettingsWindow::bind_bool(const std::string &label, bool &state)
{
  auto *check_box = new QCheckBox();
  check_box->setChecked(state);

  this->connect(check_box,
                &QCheckBox::toggled,
                this,
                [&state](bool value) { state = value; });

  this->layout->addRow(label.c_str(), check_box);
}

void AppSettingsWindow::bind_qcolor(const std::string &label, QColor &color)
{
  auto *button = new QPushButton(this);
  button->setFlat(true);
  button->setMinimumHeight(24);

  button->setStyleSheet(
      QString("background-color: %1; border: 1px solid #444;").arg(color.name()));

  this->connect(
      button,
      &QPushButton::clicked,
      this,
      [this, button, &color]()
      {
        QColor new_color = QColorDialog::getColor(color,
                                                  this,
                                                  "Select color",
                                                  QColorDialog::ShowAlphaChannel);

        if (!new_color.isValid())
          return;

        color = new_color;
        button->setStyleSheet(
            QString("background-color: %1; border: 1px solid #444;").arg(color.name()));
      });

  this->layout->addRow(label.c_str(), button);
}

void AppSettingsWindow::setup_layout()
{
  Logger::log()->trace("AppSettingsWindow::setup_layout");

  AppContext &ctx = HSD_CTX;

  this->layout = new QFormLayout(this);
  this->layout->setHorizontalSpacing(24);

  {
    QLabel *label = new QLabel;
    label->setPixmap(QIcon(ctx.app_settings.global.icon_path.c_str()).pixmap(64, 64));
    this->layout->addRow(label);
  }

  this->add_title("Hesiod - Application Settings", 4);

  this->add_description("Adjust the behavior and appearance of the application. Some "
                        "changes require a restart to take effect.");
  this->add_description("\n");

  // --- Global

  this->add_title("Global");

  this->bind_bool("Create a backup file whenever saving",
                  ctx.app_settings.global.save_backup_file);
  this->bind_int("Number of threads used for OpenMP",
                 ctx.app_settings.global.omp_num_threads);
  this->add_description("\n");

  // --- Interface

  this->add_title("Interface");

  this->bind_bool("Enable data preview in node body",
                  ctx.app_settings.interface.enable_data_preview_in_node_body);
  this->bind_bool("Enable node settings in node body",
                  ctx.app_settings.interface.enable_node_settings_in_node_body);
  this->bind_bool("Enable tool tips", ctx.app_settings.interface.enable_tool_tips);
  this->bind_bool("Enable texture downloader",
                  ctx.app_settings.interface.enable_texture_downloader);
  this->bind_bool("Enable example selector at startup",
                  ctx.app_settings.interface.enable_example_selector_at_startup);

  this->add_title("Node Editor");

  this->bind_bool("Show node toolbar in settings pan",
                  ctx.app_settings.node_editor.show_node_toolbar_in_settings_pan);
  this->bind_bool("Show node library pan",
                  ctx.app_settings.node_editor.show_node_library_pan);

  bool show_node_library_pan = true;

  this->add_title("Viewport");

  this->bind_bool("Add border skirt to the heightmap",
                  ctx.app_settings.viewer.add_heighmap_skirt);

  // --- Reset

  this->add_description("\n");

  auto *reset_button = new QPushButton("Reset Settings");
  reset_button->setMinimumHeight(32);

  reset_button->setStyleSheet("font-weight: bold; "
                              "background-color: #552222; "
                              "color: white;");

  this->connect(reset_button,
                &QPushButton::clicked,
                this,
                [this, &ctx]()
                {
                  QMessageBox::StandardButton reply = QMessageBox::warning(
                      this,
                      "Reset Settings",
                      "This will reset all application settings to their "
                      "default values.\n\n"
                      "This action cannot be undone.",
                      QMessageBox::Yes | QMessageBox::Cancel,
                      QMessageBox::Cancel);

                  if (reply != QMessageBox::Yes)
                    return;

                  ctx.reset_settings();

                  QMessageBox::information(
                      this,
                      "Settings Reset",
                      "Settings have been reset.\n"
                      "Please restart the application for all changes to take effect.");
                });

  this->layout->addRow(reset_button);
}

} // namespace hesiod
