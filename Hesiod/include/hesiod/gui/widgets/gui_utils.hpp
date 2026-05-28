/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <filesystem>
#include <functional>

#include <QApplication>
#include <QGridLayout>
#include <QImage>
#include <QLayout>
#include <QWidget>

namespace hesiod
{

void add_qmenu_spacer(QMenu *menu, int height = 8);

void apply_global_style(QApplication &app);

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
