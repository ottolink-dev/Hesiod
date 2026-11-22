/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <QAbstractButton>

#include "hesiod/gui/widgets/post_process/post_effect_glyph.hpp"
#include "hesiod/model/nodes/post_effects.hpp"

namespace meta::qt
{
class Glide;
struct Theme;
} // namespace meta::qt

namespace hesiod
{

/** @brief The display at the head of a stage.
 *
 * Shows what the stage is currently doing to the signal, and doubles as its
 * remove button: under the pointer the trace gives way to a cross. A separate
 * button for that would cost the slider next to it a fifth of its rail.
 *
 * Drawn as a value field is, in the same grays: the surface and outline of the
 * number at the other end of the row, with the icon ink for the trace. A dark
 * well with an accent trace made a column of these the heaviest thing in the
 * panel.
 */
class PostStageScope final : public QAbstractButton
{
public:
  PostStageScope(PostEffect effect, const meta::qt::Theme &theme, QWidget *parent);

  void set_glyph(const PostEffectGlyph &new_glyph);

protected:
  void enterEvent(QEnterEvent *event) override;

  void leaveEvent(QEvent *event) override;

  void focusInEvent(QFocusEvent *event) override;

  void focusOutEvent(QFocusEvent *event) override;

  void paintEvent(QPaintEvent *) override;

private:
  const PostEffect       effect;
  const meta::qt::Theme &theme;
  PostEffectGlyph        glyph;
  meta::qt::Glide       *armed = nullptr; ///< 0 showing the trace, 1 the cross
};

} // namespace hesiod
