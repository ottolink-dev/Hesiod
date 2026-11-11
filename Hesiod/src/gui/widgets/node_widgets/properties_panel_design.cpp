/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "meta_qt/designs/industrial/industrial.hpp"
#include "meta_qt/designs/stock/stock.hpp"
#include "meta_qt/ui/design_registry.hpp"
#include "meta_qt/ui/theme.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/node_widgets/properties_panel_design.hpp"
#include "hesiod/logger.hpp"

namespace hesiod
{

const PropertiesPanelDesign &properties_panel_design()
{
  static bool designs_registered = false;
  if (!designs_registered)
  {
    // Designs register once per process. Both of them, so that stock is a
    // usable fallback rather than another name that resolves to nothing.
    meta::qt::industrial::register_design();
    meta::qt::stock::register_design();
    designs_registered = true;
  }

  static PropertiesPanelDesign current;

  const AppSettings::Interface &settings = HSD_CTX.app_settings.interface;

  current.design = settings.properties_panel_design;

  if (!meta::qt::DesignRegistry::instance().has_design(current.design))
  {
    Logger::log()->warn(
        "properties_panel_design: '{}' is not a registered design, falling "
        "back to '{}'. Check interface.properties_panel_design in "
        "hesiod.json.",
        current.design,
        meta::qt::stock::kDesignName);

    current.design = meta::qt::stock::kDesignName;
  }

  // Designs name their own theme when they register. Point the chosen one at
  // whatever the settings ask for so the registry stays the single answer to
  // "what is this design drawn with", rather than the panel reading a theme
  // the design itself knows nothing about.
  if (!settings.properties_panel_theme.empty())
    meta::qt::DesignRegistry::instance().set_theme(current.design,
                                                   settings.properties_panel_theme);

  current.theme = &meta::qt::DesignRegistry::instance().theme(current.design);
  current.has_own_chrome = current.design == meta::qt::industrial::kDesignName;

  return current;
}

} // namespace hesiod
