/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/gui/theme/oklch.hpp"

#include <algorithm>
#include <cmath>

namespace hesiod::oklch
{

namespace
{

constexpr double kPi = 3.14159265358979323846;

struct Rgb
{
  double r, g, b;
};

struct Oklab
{
  double L, a, b;
};

double srgb_decode(double x)
{
  return x <= 0.04045 ? x / 12.92 : std::pow((x + 0.055) / 1.055, 2.4);
}

double srgb_encode(double x)
{
  return x <= 0.0031308 ? 12.92 * x : 1.055 * std::pow(x, 1.0 / 2.4) - 0.055;
}

Oklab linear_to_oklab(const Rgb &c)
{
  const double l = 0.4122214708 * c.r + 0.5363325363 * c.g + 0.0514459929 * c.b;
  const double m = 0.2119034982 * c.r + 0.6806995451 * c.g + 0.1073969566 * c.b;
  const double s = 0.0883024619 * c.r + 0.2817188376 * c.g + 0.6299787005 * c.b;

  const double l_ = std::cbrt(l);
  const double m_ = std::cbrt(m);
  const double s_ = std::cbrt(s);

  return {0.2104542553 * l_ + 0.7936177850 * m_ - 0.0040720468 * s_,
          1.9779984951 * l_ - 2.4285922050 * m_ + 0.4505937099 * s_,
          0.0259040371 * l_ + 0.7827717662 * m_ - 0.8086757660 * s_};
}

Rgb oklab_to_linear(const Oklab &c)
{
  const double l_ = c.L + 0.3963377774 * c.a + 0.2158037573 * c.b;
  const double m_ = c.L - 0.1055613458 * c.a - 0.0638541728 * c.b;
  const double s_ = c.L - 0.0894841775 * c.a - 1.2914855480 * c.b;

  const double l = l_ * l_ * l_;
  const double m = m_ * m_ * m_;
  const double s = s_ * s_ * s_;

  return {+4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
          -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
          -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s};
}

Oklab to_oklab(const QColor &color)
{
  return linear_to_oklab({srgb_decode(color.redF()),
                          srgb_decode(color.greenF()),
                          srgb_decode(color.blueF())});
}

bool in_gamut(const Rgb &c)
{
  constexpr double eps = 1e-4;
  return c.r >= -eps && c.r <= 1.0 + eps && c.g >= -eps && c.g <= 1.0 + eps &&
         c.b >= -eps && c.b <= 1.0 + eps;
}

QColor encode(const Rgb &c, double alpha)
{
  QColor out;
  out.setRgbF(
      static_cast<float>(std::clamp(srgb_encode(std::clamp(c.r, 0.0, 1.0)), 0.0, 1.0)),
      static_cast<float>(std::clamp(srgb_encode(std::clamp(c.g, 0.0, 1.0)), 0.0, 1.0)),
      static_cast<float>(std::clamp(srgb_encode(std::clamp(c.b, 0.0, 1.0)), 0.0, 1.0)),
      static_cast<float>(std::clamp(alpha, 0.0, 1.0)));
  return out;
}

Oklab lch_to_oklab(const Lch &c)
{
  const double rad = c.h * kPi / 180.0;
  return {c.L, c.C * std::cos(rad), c.C * std::sin(rad)};
}

double luminance(const QColor &c)
{
  return 0.2126 * srgb_decode(c.redF()) + 0.7152 * srgb_decode(c.greenF()) +
         0.0722 * srgb_decode(c.blueF());
}

} // namespace

Lch to_lch(const QColor &color)
{
  const Oklab c = to_oklab(color);
  Lch         out;
  out.L = c.L;
  out.C = std::hypot(c.a, c.b);
  out.h = std::atan2(c.b, c.a) * 180.0 / kPi;
  if (out.h < 0.0)
    out.h += 360.0;
  return out;
}

QColor from_lch(const Lch &lch, double alpha)
{
  Lch c = lch;
  c.L = std::clamp(c.L, 0.0, 1.0);
  c.C = std::max(c.C, 0.0);

  Rgb rgb = oklab_to_linear(lch_to_oklab(c));
  if (in_gamut(rgb))
    return encode(rgb, alpha);

  // keep lightness and hue, give up chroma until the colour exists in sRGB
  double lo = 0.0;
  double hi = c.C;
  for (int i = 0; i < 18; ++i)
  {
    const double mid = 0.5 * (lo + hi);
    Lch          probe = c;
    probe.C = mid;
    if (in_gamut(oklab_to_linear(lch_to_oklab(probe))))
      lo = mid;
    else
      hi = mid;
  }
  c.C = lo;
  return encode(oklab_to_linear(lch_to_oklab(c)), alpha);
}

QColor mix(const QColor &a, const QColor &b, double t)
{
  t = std::clamp(t, 0.0, 1.0);
  const Oklab x = to_oklab(a);
  const Oklab y = to_oklab(b);
  const Oklab m{x.L + (y.L - x.L) * t, x.a + (y.a - x.a) * t, x.b + (y.b - x.b) * t};
  return encode(oklab_to_linear(m), a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

double contrast(const QColor &a, const QColor &b)
{
  const double la = luminance(a);
  const double lb = luminance(b);
  return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

QColor ensure_contrast(const QColor &fg, const QColor &bg, double ratio)
{
  if (contrast(fg, bg) >= ratio)
    return fg;

  Lch          c = to_lch(fg);
  const double direction = luminance(bg) < 0.18 ? 1.0 : -1.0;
  for (int i = 0; i < 60; ++i)
  {
    c.L = std::clamp(c.L + direction * 0.01, 0.0, 1.0);
    const QColor probe = from_lch(c, fg.alphaF());
    if (contrast(probe, bg) >= ratio)
      return probe;
  }
  return from_lch(c, fg.alphaF());
}

QColor shifted(const QColor &color, double delta)
{
  Lch c = to_lch(color);
  c.L = std::clamp(c.L + delta, 0.0, 1.0);
  return from_lch(c, color.alphaF());
}

} // namespace hesiod::oklch
