/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cmath>

#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/app/ui_scale.hpp"
#include "hesiod/gui/widgets/app_settings_window.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/gui/widgets/node_palette_sidebar.hpp"
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
  label->setWordWrap(true); // a pre-wrapped line still has to reflow when the
                            // dialog is narrower than the text it was wrapped for

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

void AppSettingsWindow::bind_bool(const std::string        &label,
                                  bool                     &state,
                                  std::function<void(bool)> on_changed)
{
  auto *check_box = new QCheckBox();
  check_box->setChecked(state);

  this->connect(check_box,
                &QCheckBox::toggled,
                this,
                [&state, on_changed](bool value)
                {
                  state = value;
                  if (on_changed)
                    on_changed(value);
                });

  this->layout->addRow(label.c_str(), check_box);
}

void AppSettingsWindow::bind_int_range(const std::string &label,
                                       int               &value,
                                       int                min,
                                       int                max,
                                       const std::string &suffix)
{
  auto *spin_box = new QSpinBox();
  spin_box->setRange(min, max);
  spin_box->setValue(std::clamp(value, min, max));
  spin_box->setSuffix(suffix.c_str());

  this->connect(spin_box,
                QOverload<int>::of(&QSpinBox::valueChanged),
                this,
                [this, &value](int v)
                {
                  value = v;
                  NodePaletteSidebar::restyle_all(current_node_palette_style());
                });

  this->layout->addRow(label.c_str(), spin_box);
}

void AppSettingsWindow::refresh_theme()
{
  // colours reach three places: the global stylesheet and palette, the icons
  // (re-tinted inside apply_global_style) and any open palette sidebar
  if (auto *app = qobject_cast<QApplication *>(QCoreApplication::instance()))
    apply_global_style(*app);

  NodePaletteSidebar::restyle_all(current_node_palette_style());
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

        this->refresh_theme();
      });

  this->layout->addRow(label.c_str(), button);
}

void AppSettingsWindow::setup_interface_scale_row()
{
  AppContext &ctx = HSD_CTX;

  auto *spin_box = new QDoubleSpinBox();
  spin_box->setRange(ui_scale::kMin, ui_scale::kMax);
  spin_box->setSingleStep(ui_scale::kStep);
  spin_box->setDecimals(2);
  spin_box->setSuffix(" x");
  spin_box->setValue(ui_scale::sanitize(ctx.app_settings.interface.ui_scale));
  spin_box->setToolTip("Scales the whole interface: text, icons, spacing and "
                       "controls. Multiplied with the monitor's own scaling "
                       "rather than replacing it.");

  auto *reset_button = new QPushButton("Reset");
  reset_button->setToolTip("Back to 1.00x");

  auto *note = new QLabel();
  note->setWordWrap(true);
  resize_font(note, -1);

  auto update_note = [note, &ctx]()
  {
    const double configured = ui_scale::sanitize(ctx.app_settings.interface.ui_scale);
    const ui_scale::Resolution session = ui_scale::session_resolution();
    const double               running = session.effective;

    QString text;
    bool    highlight = false;

    if (session.environment_override)
    {
      // Saying "restart to apply" here would be a lie: Qt is obeying the
      // environment and a restart changes nothing until that variable is gone.
      text = QString("QT_SCALE_FACTOR is set in this process's environment, so "
                     "Hesiod is running at %1x and this setting is not being "
                     "applied. Clear that variable to use the setting.")
                 .arg(running, 0, 'f', 2);
      highlight = true;
    }
    else if (std::abs(configured - running) > 1e-6)
    {
      text = QString("Running at %1x. Restart Hesiod to apply %2x.")
                 .arg(running, 0, 'f', 2)
                 .arg(configured, 0, 'f', 2);
      highlight = true;
    }
    else
    {
      text = QString("Running at %1x.").arg(running, 0, 'f', 2);
    }

    note->setText(text);
    note->setStyleSheet(
        QString("color: %1;")
            .arg(highlight ? ctx.app_settings.colors.accent.name()
                           : ctx.app_settings.colors.text_secondary.name()));
  };

  update_note();

  this->connect(spin_box,
                QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this,
                [&ctx, update_note](double value)
                {
                  // store the sanitized absolute value, never a delta: two
                  // changes in a row must land where a single jump would
                  ctx.app_settings.interface.ui_scale = ui_scale::sanitize(value);
                  update_note();
                });

  this->connect(reset_button,
                &QPushButton::clicked,
                this,
                [spin_box]() { spin_box->setValue(ui_scale::kDefault); });

  auto *row = new QWidget();
  auto *row_layout = new QHBoxLayout(row);
  row_layout->setContentsMargins(0, 0, 0, 0);
  row_layout->addWidget(spin_box, 1);
  row_layout->addWidget(reset_button, 0);

  this->layout->addRow("Interface scale", row);
  this->layout->addRow(note);
}

void AppSettingsWindow::setup_layout()
{
  Logger::log()->trace("AppSettingsWindow::setup_layout");

  AppContext &ctx = HSD_CTX;

  this->layout = new QFormLayout(this);
  this->layout->setHorizontalSpacing(24);

  // At a large interface scale the label column plus its field no longer fit
  // side by side; without this the form just gets wider than the screen and the
  // controls go off the right edge instead of stacking under their labels.
  this->layout->setRowWrapPolicy(QFormLayout::WrapLongRows);
  this->layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

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

  this->bind_bool("Enable interface animations",
                  ctx.app_settings.interface.enable_ui_animations,
                  [](bool enabled) { apply_animation_settings(enabled); });

  this->add_description("\n");

  // --- Interface scale

  this->add_title("Interface Scale");
  this->setup_interface_scale_row();

  this->add_description("\n");

  // --- Node palette sidebar

  this->add_title("Node Palette Sidebar");

  this->add_description("An alternative to the node library tree: a category "
                        "rail with hierarchical flyout menus. Takes effect on the "
                        "next restart.");

  this->bind_bool("Use the category rail instead of the library tree",
                  ctx.app_settings.interface.enable_node_palette_sidebar);

  this->bind_qcolor("Category button", ctx.app_settings.node_palette.surface);
  this->bind_qcolor("Category button (hovered)",
                    ctx.app_settings.node_palette.surface_hover);
  this->bind_qcolor("Category button (open)",
                    ctx.app_settings.node_palette.surface_selected);
  this->bind_qcolor("Flyout background", ctx.app_settings.node_palette.flyout_bg);
  this->bind_qcolor("Flyout border", ctx.app_settings.node_palette.flyout_border);
  this->bind_qcolor("Label", ctx.app_settings.node_palette.text);
  this->bind_qcolor("Label (active)", ctx.app_settings.node_palette.text_active);

  this->bind_int_range("Category button height",
                       ctx.app_settings.node_palette.button_height,
                       20,
                       120);
  this->bind_int_range("Category button spacing",
                       ctx.app_settings.node_palette.button_spacing,
                       0,
                       40);
  this->bind_int_range("Rail padding", ctx.app_settings.node_palette.rail_padding, 0, 40);
  this->bind_int_range("Category icon size",
                       ctx.app_settings.node_palette.icon_size,
                       8,
                       64);
  this->bind_int_range("Corner radius",
                       ctx.app_settings.node_palette.corner_radius,
                       0,
                       24);
  this->bind_int_range("Flyout row height",
                       ctx.app_settings.node_palette.flyout_row_height,
                       14,
                       80);
  this->bind_int_range("Flyout padding",
                       ctx.app_settings.node_palette.flyout_padding,
                       0,
                       24);
  this->bind_int_range("Animation duration",
                       ctx.app_settings.node_palette.animation_ms,
                       0,
                       1000,
                       " ms");

  this->add_description("\n");

  this->add_title("Node Editor");

  this->bind_bool("Show node toolbar in settings pan",
                  ctx.app_settings.node_editor.show_node_toolbar_in_settings_pan);
  this->bind_bool("Show node library pan",
                  ctx.app_settings.node_editor.show_node_library_pan);
  this->bind_bool("Enable live update during editing",
                  ctx.app_settings.node_editor.live_update);

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
