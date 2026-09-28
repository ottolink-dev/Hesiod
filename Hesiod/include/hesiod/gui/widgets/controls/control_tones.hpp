/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QColor>

namespace hesiod
{

// The colours the settings-style controls are painted with, derived from the
// application palette (AppSettings::colors) each time, so a theme change is
// picked up on the next paint.
struct ControlTones
{
  QColor sidebar, content, card, border, field, field_hover, ink, ink_dim, ink_faint,
      accent, accent_soft;
};

ControlTones control_tones();

} // namespace hesiod
