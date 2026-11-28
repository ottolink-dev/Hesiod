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

/** @brief The mark ahead of a node's name in the library's flyouts and search
 *  results, drawn into @p box.
 *
 * Hesiod has no per-node icons yet, so a node wears its category's glyph.
 * This is the one place to change when it does: the flyout card and the
 * search results both draw through it.
 *
 * @param node_type the node, or a sub-category's path for a sub-list's row
 * @param category the top-level category (or the sub-category's own name)
 */
void draw_node_glyph(QPainter      &painter,
                     const QRectF  &box,
                     const QColor  &color,
                     const QString &node_type,
                     const QString &category);

} // namespace hesiod
