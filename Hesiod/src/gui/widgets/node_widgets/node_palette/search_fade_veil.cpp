/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include <QPainter>

#include "hesiod/gui/widgets/node_widgets/node_palette/node_palette_style.hpp"
#include "hesiod/gui/widgets/node_widgets/node_palette/search_fade_veil.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"

namespace hesiod
{

SearchFadeVeil::SearchFadeVeil(QWidget *parent) : QWidget(parent)
{
  this->setAttribute(Qt::WA_TransparentForMouseEvents);
  this->setAttribute(Qt::WA_NoSystemBackground);
  this->setFocusPolicy(Qt::NoFocus);
}

void SearchFadeVeil::paintEvent(QPaintEvent *)
{
  if (this->veil_alpha <= node_palette::hidden_below)
    return;

  QPainter painter(this);
  painter.fillRect(this->rect(), with_alpha(this->ground, this->veil_alpha));
}

void SearchFadeVeil::set_state(const QColor &new_ground, double opacity)
{
  this->ground = new_ground;
  this->veil_alpha = std::clamp(opacity, 0.0, 1.0);
  this->setVisible(this->veil_alpha > node_palette::hidden_below);
  if (this->isVisible())
    this->update();
}

} // namespace hesiod
