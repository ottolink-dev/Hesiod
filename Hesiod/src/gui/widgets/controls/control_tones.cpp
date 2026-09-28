/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/gui/widgets/controls/control_tones.hpp"
#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"

namespace hesiod
{

ControlTones control_tones()
{
  const auto  &c = HSD_CTX.app_settings.colors;
  ControlTones t;
  t.sidebar = c.bg_deep;
  t.content = mix_colors(c.bg_deep, c.bg_primary, 0.40);
  t.card = c.bg_primary;
  t.border = panel_border_color();
  t.field = mix_colors(c.bg_deep, c.bg_primary, 0.55);
  t.field_hover = mix_colors(c.bg_primary, c.border, 0.25);
  t.ink = c.text_primary;
  t.ink_dim = mix_colors(c.bg_primary, c.text_primary, 0.62);
  t.ink_faint = mix_colors(c.bg_primary, c.text_primary, 0.42);
  t.accent = c.accent;
  t.accent_soft = mix_colors(c.bg_primary, c.accent, 0.30);
  return t;
}

} // namespace hesiod
