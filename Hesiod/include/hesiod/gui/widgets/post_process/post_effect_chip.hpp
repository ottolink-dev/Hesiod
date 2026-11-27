/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <functional>

#include <QAbstractButton>

#include "hesiod/model/nodes/post_effects.hpp"

namespace meta::qt
{
class Glide;
struct Theme;
} // namespace meta::qt

namespace hesiod
{

/// One effect on the tray: lit when it is in the chain, a click moves it in or out.
class PostEffectChip final : public QAbstractButton
{
public:
  PostEffectChip(PostEffect effect, const meta::qt::Theme &theme, QWidget *parent);

  const PostEffect          effect;
  std::function<void(bool)> on_hover;

protected:
  void enterEvent(QEnterEvent *event) override;

  void leaveEvent(QEvent *event) override;

  void paintEvent(QPaintEvent *) override;

private:
  const meta::qt::Theme &theme;
  meta::qt::Glide       *lit = nullptr;   ///< 0 off, 1 in the chain
  meta::qt::Glide       *hover = nullptr; ///< 0 away, 1 under the pointer
  bool                   lit_target = false;
  bool                   seated = false;
};

} // namespace hesiod
