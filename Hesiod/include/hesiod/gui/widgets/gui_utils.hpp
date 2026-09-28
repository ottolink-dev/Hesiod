/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <filesystem>
#include <functional>

#include <QApplication>
#include <QColor>
#include <QGridLayout>
#include <QImage>
#include <QLayout>
#include <QWidget>

#include "hesiod/gui/node_palette_style.hpp"

namespace hesiod
{

void add_qmenu_spacer(QMenu *menu, int height = 8);

void apply_global_style(QApplication &app);

// Linear blend of two colours, alpha included; `amount` is clamped to [0, 1].
QColor mix_colors(const QColor &from, const QColor &to, qreal amount);

// Border of the rounded pane cards, and of the popups and dialogs drawn to
// match them. The stylesheet's COLOR_PANEL_BORDER is this same colour.
QColor panel_border_color();

/** @brief Switch interface motion on or off across the application.
 *
 * Covers Qt's own menu/combo/tooltip effects and any node palette sidebar that
 * is already open. Called at startup and again whenever the setting changes,
 * because "disable animations" that only applies to windows opened afterwards
 * reads as the setting not working.
 */
void apply_animation_settings(bool enabled);

/// The node palette style currently configured in the application settings.
NodePaletteStyle current_node_palette_style();

void clear_layout(QLayout *layout);

int get_column_count(QGridLayout *layout);
int get_row_count(QGridLayout *layout);

void resize_font(QWidget *widget, int relative_size_modification);

// to the widget only, block propagation
void set_style(QWidget *widget, const std::string &style);

void render_widget_screenshot(QWidget              *widget,
                              const std::string    &fname,
                              const QSize          &size = QSize(),
                              std::function<void()> post_render_callback = nullptr);

int   float_to_slider_pos(float v, float min, float max, int slider_steps);
float slider_pos_to_float(int pos, float min, float max, int slider_steps);

bool save_heightmap(const QImage                &src,
                    const std::filesystem::path &path,
                    float                        aspectRatio);

} // namespace hesiod
