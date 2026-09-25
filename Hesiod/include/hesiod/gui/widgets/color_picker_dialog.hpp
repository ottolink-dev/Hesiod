/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QColor>

#include "hesiod/gui/widgets/message_dialog.hpp"

class QLineEdit;
class QSpinBox;

namespace hesiod
{

class ColorPlane;
class ColorSlider;
class ColorPreview;

// =====================================
// ColorPickerDialog
// =====================================

// Colour picker in the application's dialog chrome, replacing QColorDialog:
// a saturation / value plane, hue and (optionally) alpha sliders, an old / new
// preview, hex and RGB(A) fields, and quick swatches (the theme colours and the
// last colours picked this session).
//
// Hue is kept separately from the RGB value, so dragging through greys and
// black does not lose it.
class ColorPickerDialog : public MessageDialog
{
public:
  ColorPickerDialog(const QColor  &initial,
                    QWidget       *parent = nullptr,
                    const QString &title = QString(),
                    bool           alpha = false);

  QColor color() const;

  /// QColorDialog::getColor equivalent: an invalid colour when cancelled.
  static QColor get_color(const QColor  &initial,
                          QWidget       *parent = nullptr,
                          const QString &title = QString(),
                          bool           alpha = false);

private:
  enum class Source
  {
    None,
    Plane,
    Hue,
    Alpha,
    Hex,
    Channels,
    Swatch
  };

  void set_hsv(qreal h, qreal s, qreal v, qreal a, Source source);
  void set_from_color(const QColor &color, Source source);

  qreal hue = 0.0, sat = 0.0, val = 0.0, alpha_value = 1.0;
  bool  with_alpha = false;

  ColorPlane   *plane = nullptr;
  ColorSlider  *hue_slider = nullptr;
  ColorSlider  *alpha_slider = nullptr;
  ColorPreview *preview = nullptr;
  QLineEdit    *hex = nullptr;
  QSpinBox     *channels[4] = {nullptr, nullptr, nullptr, nullptr};
};

// Makes ColorPickerDialog the picker used by MetaUI widgets (node colour
// attributes, gradient stops) as well.
void install_color_picker();

} // namespace hesiod
