/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <functional>
#include <string>

#include <QFormLayout>
#include <QObject>
#include <QWidget>

namespace hesiod
{

class AppSettingsWindow : public QWidget
{
  Q_OBJECT
public:
  explicit AppSettingsWindow(QWidget *parent = nullptr);

private:
  void setup_layout();
  void setup_interface_scale_row();

  void add_description(const std::string &description, int max_length = 64);
  void add_title(const std::string &label, int font_size_delta = 2);
  void bind_bool(const std::string    &label,
                 bool                 &state,
                 std::function<void(bool)> on_changed = nullptr);
  void bind_int(const std::string &label, int &value);
  /// Spin box over an explicit range, for a metric with real bounds.
  void bind_int_range(const std::string &label,
                      int               &value,
                      int                min,
                      int                max,
                      const std::string &suffix = " px");
  void bind_qcolor(const std::string &label, QColor &color);

  /// Repaint the running application after a colour changed, so the setting is
  /// visible without a restart.
  void refresh_theme();

  QFormLayout *layout;
};

} // namespace hesiod