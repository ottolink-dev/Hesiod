/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <QColor>
#include <QRectF>
#include <QString>

class QPainter;

namespace hesiod
{

/** @brief One of the node library's line glyphs, drawn into @p box.
 *
 * Hesiod ships no per-category artwork and inventing forty SVGs for a togglable
 * sidebar is not worth it. Drawing them keeps the set consistent and scales
 * with the interface scale for free.
 */
void draw_category_glyph(QPainter     &painter,
                         const QRectF &box,
                         const QColor &color,
                         int           shape);

/// The glyph of a category: its own for the categories Hesiod ships, otherwise
/// a stable function of the name.
int category_glyph_shape(const QString &category);

} // namespace hesiod
