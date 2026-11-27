/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cmath>
#include <functional>

#include <QPainter>
#include <QPainterPath>

#include "meta/core/attribute.hpp"
#include "meta/core/attribute_container.hpp"
#include "meta_common.hpp"

#include "hesiod/gui/widgets/post_process/post_effect_glyph.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"

namespace hesiod
{

PostEffectGlyph canonical_post_glyph(PostEffect effect)
{
  switch (effect)
  {
  case PostEffect::GAMMA:
    return {2.4f, 0.f};
  case PostEffect::GAIN:
    return {2.6f, 0.f};
  case PostEffect::SMOOTHING:
    return {0.35f, 0.f};
  case PostEffect::REMAP:
    return {0.06f, 0.94f};
  case PostEffect::SATURATE:
    return {0.3f, 0.7f};
  default:
    return {};
  }
}

PostEffectGlyph model_post_glyph(const meta::AttributeContainer &container,
                                 PostEffect                      effect)
{
  PostEffectGlyph glyph = canonical_post_glyph(effect);

  const auto span = [&container, &glyph](const char *key)
  {
    const meta::AbstractAttribute *p_attr = container.find(key);
    const auto *p_range = p_attr ? p_attr->try_cast<meta::Attribute<glm::vec2>>()
                                 : nullptr;
    if (!p_range)
      return;

    const float lo = meta::common::min<float>(*p_range);
    const float hi = meta::common::max<float>(*p_range);
    if (!(hi > lo))
      return;

    glyph.a = (p_range->value().x - lo) / (hi - lo);
    glyph.b = (p_range->value().y - lo) / (hi - lo);
  };

  switch (effect)
  {
  case PostEffect::GAMMA:
    if (const float *p = container.try_value<float>(post_keys::GAMMA))
      glyph.a = *p;
    break;

  case PostEffect::GAIN:
    if (const float *p = container.try_value<float>(post_keys::GAIN))
      glyph.a = *p;
    break;

  case PostEffect::SMOOTHING:
  {
    const meta::AbstractAttribute *p_attr = container.find(post_keys::SMOOTHING_RADIUS);
    const auto *p_radius = p_attr ? p_attr->try_cast<meta::Attribute<float>>() : nullptr;
    if (p_radius)
    {
      const float radius_max = meta::common::max(*p_radius);
      glyph.a = radius_max > 0.f ? p_radius->value() / radius_max : 0.f;
    }
    break;
  }

  case PostEffect::REMAP:
    span(post_keys::REMAP);
    break;
  case PostEffect::SATURATE:
    span(post_keys::SATURATE);
    break;
  default:
    break;
  }

  return glyph;
}

void paint_post_glyph(QPainter              &painter,
                      const QRectF          &box,
                      PostEffect             effect,
                      const PostEffectGlyph &glyph,
                      const QColor          &ink)
{
  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setPen(QPen(ink, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.setBrush(Qt::NoBrush);

  // the unit square, y up
  const auto at = [&box](qreal x, qreal y)
  { return QPointF(box.left() + x * box.width(), box.bottom() - y * box.height()); };

  const auto trace = [&painter, &at](const std::function<qreal(qreal)> &f)
  {
    constexpr int steps = 24;
    QPainterPath  path;

    for (int i = 0; i <= steps; ++i)
    {
      const qreal   x = qreal(i) / steps;
      const QPointF point = at(x, std::clamp(f(x), 0.0, 1.0));
      if (i == 0)
        path.moveTo(point);
      else
        path.lineTo(point);
    }

    painter.drawPath(path);
  };

  switch (effect)
  {
  case PostEffect::MIX:
  {
    const qreal   r = 0.32 * box.width();
    const QPointF left(box.center().x() - 0.56 * r, box.center().y());
    const QPointF right(box.center().x() + 0.56 * r, box.center().y());

    QPainterPath a;
    QPainterPath b;
    a.addEllipse(left, r, r);
    b.addEllipse(right, r, r);

    painter.fillPath(a.intersected(b), with_alpha(ink, 0.5 * ink.alphaF()));
    painter.drawPath(a);
    painter.drawPath(b);
    break;
  }

  case PostEffect::INVERT:
  {
    const qreal  r = 0.42 * box.width();
    const QRectF disc(box.center().x() - r, box.center().y() - r, 2 * r, 2 * r);

    QPainterPath half;
    half.moveTo(disc.center().x(), disc.top());
    half.arcTo(disc, 90, -180);
    half.closeSubpath();

    painter.fillPath(half, ink);
    painter.drawEllipse(disc);
    break;
  }

  case PostEffect::GAMMA:
  {
    const qreal gamma = std::max(0.01f, glyph.a);
    trace([gamma](qreal x) { return std::pow(x, gamma); });
    break;
  }

  case PostEffect::GAIN:
  {
    // hmap::gain
    const qreal gain = std::max(0.01f, glyph.a);
    trace(
        [gain](qreal x)
        {
          return x < 0.5 ? 0.5 * std::pow(2.0 * x, gain)
                         : 1.0 - 0.5 * std::pow(2.0 * (1.0 - x), gain);
        });
    break;
  }

  case PostEffect::SMOOTHING:
  {
    // narrow and tall with no radius to speak of, low and wide with a lot
    const qreal t = std::clamp(qreal(glyph.a), 0.0, 1.0);
    const qreal sigma = 0.09 + 0.26 * t;
    const qreal peak = 0.94 - 0.42 * t;
    trace(
        [sigma, peak](qreal x)
        {
          const qreal d = (x - 0.5) / sigma;
          return 0.06 + (peak - 0.06) * std::exp(-d * d);
        });
    break;
  }

  case PostEffect::REMAP:
  {
    qreal lo = std::clamp(qreal(glyph.a), 0.0, 1.0);
    qreal hi = std::clamp(qreal(glyph.b), 0.0, 1.0);

    // two levels have to stay two levels however narrow the range gets
    if (hi - lo < 0.3)
    {
      const qreal mid = std::clamp(0.5 * (lo + hi), 0.15, 0.85);
      lo = mid - 0.15;
      hi = mid + 0.15;
    }

    painter.drawLine(at(0.08, hi), at(0.92, hi));
    painter.drawLine(at(0.08, lo), at(0.92, lo));

    if (hi - lo > 0.5)
    {
      const qreal head = 0.16;
      painter.drawLine(at(0.5, lo + 0.16), at(0.5, hi - 0.16));
      painter.drawLine(at(0.5, hi - 0.16), at(0.5 - head, hi - 0.16 - head));
      painter.drawLine(at(0.5, hi - 0.16), at(0.5 + head, hi - 0.16 - head));
      painter.drawLine(at(0.5, lo + 0.16), at(0.5 - head, lo + 0.16 + head));
      painter.drawLine(at(0.5, lo + 0.16), at(0.5 + head, lo + 0.16 + head));
    }
    break;
  }

  case PostEffect::SATURATE:
  {
    const qreal lo = std::clamp(qreal(glyph.a), 0.0, 0.9);
    const qreal hi = std::clamp(qreal(glyph.b), lo + 0.1, 1.0);
    trace([lo, hi](qreal x)
          { return 0.12 + 0.76 * std::clamp((x - lo) / (hi - lo), 0.0, 1.0); });
    break;
  }
  }

  painter.restore();
}

} // namespace hesiod
