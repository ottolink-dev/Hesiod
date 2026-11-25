/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
// OKLab / OKLCH color math (Björn Ottosson's matrices), no dependency.
// Everything the theme derives goes through here so that "a bit lighter" and
// "half way between" mean the same thing for every colorway. A neutral gray
// (r == g == b) stays exactly neutral through every helper.
#pragma once
#include <QColor>

namespace hesiod::oklch
{

struct Lch
{
  double L = 0.0; // 0..1
  double C = 0.0; // ~0..0.37
  double h = 0.0; // degrees
};

Lch    to_lch(const QColor &color);
QColor from_lch(const Lch &lch,
                double     alpha = 1.0); // chroma is reduced until it fits sRGB

// interpolation in OKLab, t = 0 gives a, t = 1 gives b (alpha interpolated too)
QColor mix(const QColor &a, const QColor &b, double t);

// WCAG contrast ratio of two opaque colors
double contrast(const QColor &a, const QColor &b);

// moves the lightness of fg away from bg until the ratio is reached
QColor ensure_contrast(const QColor &fg, const QColor &bg, double ratio);

// the same color, `delta` lighter (or darker) in perceptual lightness
QColor shifted(const QColor &color, double delta);

} // namespace hesiod::oklch
