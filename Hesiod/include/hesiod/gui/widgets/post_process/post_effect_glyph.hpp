/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <QColor>
#include <QRectF>

#include "hesiod/model/nodes/post_effects.hpp"

class QPainter;

namespace meta
{
class AttributeContainer;
}

namespace hesiod
{

/** @brief What a glyph is drawn from.
 *
 * One pair covers every effect: the exponent for gamma and gain, the radius as
 * a fraction of its range for smoothing, the two ends for the ranges.
 */
struct PostEffectGlyph
{
  float a = 0.f;
  float b = 0.f;
};

/// The shape an effect is recognised by, used on the tray.
PostEffectGlyph canonical_post_glyph(PostEffect effect);

/// The same shape, from what the node is actually set to.
PostEffectGlyph model_post_glyph(const meta::AttributeContainer &container,
                                 PostEffect                      effect);

/** @brief Draw an effect as what it does to the signal.
 *
 * Gamma, gain and saturate are their own transfer curves, input left to right
 * and output bottom to top, so the glyph on a stage bends as its slider moves.
 * Smoothing is its kernel, remap the two levels it stretches between. Invert
 * and mix have no curve worth the name and keep a pictogram.
 */
void paint_post_glyph(QPainter              &painter,
                      const QRectF          &box,
                      PostEffect             effect,
                      const PostEffectGlyph &glyph,
                      const QColor          &ink);

} // namespace hesiod
