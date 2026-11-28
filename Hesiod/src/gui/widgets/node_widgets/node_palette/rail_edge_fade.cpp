/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cmath>

#include <QLinearGradient>
#include <QPainter>

#include "hesiod/gui/widgets/node_widgets/node_palette/node_palette_style.hpp"
#include "hesiod/gui/widgets/node_widgets/node_palette/rail_edge_fade.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"

namespace hesiod
{

namespace np = node_palette;

RailEdgeFade::RailEdgeFade(QWidget *parent) : QWidget(parent)
{
  this->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  this->setAttribute(Qt::WA_NoSystemBackground, true);
}

void RailEdgeFade::paintEvent(QPaintEvent *)
{
  if (this->top <= np::hidden_below && this->bottom <= np::hidden_below)
    return;

  QPainter painter(this);

  auto band = [&](double from_y, double to_y, double amount)
  {
    QLinearGradient gradient(0.0, from_y, 0.0, to_y);
    gradient.setColorAt(0.0, with_alpha(this->ground, np::fade_solid * amount));
    gradient.setColorAt(np::fade_mid_at, with_alpha(this->ground, np::fade_mid * amount));
    gradient.setColorAt(1.0, with_alpha(this->ground, 0.0));
    painter.fillRect(QRectF(0.0, std::min(from_y, to_y), this->width(), np::fade_reach),
                     gradient);
  };

  if (this->top > np::hidden_below)
    band(0.0, np::fade_reach, this->top);
  if (this->bottom > np::hidden_below)
    band(this->height(), this->height() - np::fade_reach, this->bottom);
}

void RailEdgeFade::set_state(const QColor &new_ground, double new_top, double new_bottom)
{
  if (new_ground == this->ground && std::abs(new_top - this->top) < np::hidden_below &&
      std::abs(new_bottom - this->bottom) < np::hidden_below)
    return;

  this->ground = new_ground;
  this->top = new_top;
  this->bottom = new_bottom;
  this->update();
}

} // namespace hesiod
