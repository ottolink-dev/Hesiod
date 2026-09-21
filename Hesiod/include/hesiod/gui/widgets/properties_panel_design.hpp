/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <string>

namespace meta::qt
{
struct Theme;
}

namespace hesiod
{

/** @brief The properties panel's design, resolved once and validated.
 *
 * Both the settings pane and the attributes widget need to know which design
 * is running: one to decide its scrollbar and margins, the other to pick the
 * row renderer and whether to wrap loose attributes in a root section. Asking
 * the settings separately in each let the two disagree, so the panel ended up
 * with industrial chrome around stock rows.
 */
struct PropertiesPanelDesign
{
  /// Design name that is actually registered, never the raw settings string.
  std::string design;

  /// Theme the design draws with.
  const meta::qt::Theme *theme = nullptr;

  /// True when the design brings its own panel chrome and section cards.
  bool has_own_chrome = false;
};

/** @brief Resolve the panel design from app settings.
 *
 * Registers the built-in designs on first call, then checks the configured
 * name against the registry. An unknown name falls back to stock with a
 * warning: DesignRegistry only follows explicit fallbacks and otherwise
 * returns nullptr, so a typo in hesiod.json would otherwise drop every row and
 * leave an empty panel with nothing to explain it.
 */
const PropertiesPanelDesign &properties_panel_design();

} // namespace hesiod
