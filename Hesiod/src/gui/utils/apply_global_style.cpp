/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QPalette>

#include "gnodegui/style.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

void apply_global_style(QApplication &app)
{
  Logger::log()->trace("apply_global_style");

  std::string style_sheet =
#include "hesiod/gui/darkstyle.css"
      ;

  AppContext &ctx = HSD_CTX;

  std::map<std::string, QColor> place_holders = {
      {"COLOR_BG_DEEP", ctx.app_settings.colors.bg_deep},
      {"COLOR_BG_PRIMARY", ctx.app_settings.colors.bg_primary},
      {"COLOR_BG_SECONDARY", ctx.app_settings.colors.bg_secondary},
      {"COLOR_TEXT_PRIMARY", ctx.app_settings.colors.text_primary},
      {"COLOR_TEXT_SECONDARY", ctx.app_settings.colors.text_secondary},
      {"COLOR_TEXT_DISABLED", ctx.app_settings.colors.text_disabled},
      {"COLOR_ACCENT", ctx.app_settings.colors.accent},
      {"COLOR_BW_ACCENT", ctx.app_settings.colors.accent_bw},
      {"COLOR_BORDER", ctx.app_settings.colors.border},
      {"COLOR_HOVER", ctx.app_settings.colors.hover},
      {"COLOR_PRESSED", ctx.app_settings.colors.pressed},
      {"COLOR_SEPARATOR", ctx.app_settings.colors.separator}};

  for (auto &[p, color] : place_holders)
    hesiod::replace_all(style_sheet, p, color.name().toStdString());

  // The theme has to reach the palette as well as the stylesheet: custom-painted
  // widgets (the MetaUI sliders, range bars and canvases) query QPalette
  // directly, and a stylesheet `color:` rule only ever reaches widgets that Qt
  // itself draws. Left to the platform palette they paint near-black text on the
  // dark background and use the system accent instead of ours. This is the
  // palette-side counterpart of the qsx::Config block below.
  {
    const auto &colors = ctx.app_settings.colors;

    QPalette pal = app.palette();

    pal.setColor(QPalette::Window, colors.bg_primary);
    pal.setColor(QPalette::WindowText, colors.text_primary);
    pal.setColor(QPalette::Base, colors.bg_secondary);
    pal.setColor(QPalette::AlternateBase, colors.bg_primary);
    pal.setColor(QPalette::Text, colors.text_primary);
    pal.setColor(QPalette::PlaceholderText, colors.text_secondary);
    pal.setColor(QPalette::Button, colors.bg_secondary);
    pal.setColor(QPalette::ButtonText, colors.text_primary);
    pal.setColor(QPalette::BrightText, colors.text_primary);
    pal.setColor(QPalette::Highlight, colors.accent);
    pal.setColor(QPalette::HighlightedText, colors.text_primary);
    pal.setColor(QPalette::ToolTipBase, colors.bg_deep);
    pal.setColor(QPalette::ToolTipText, colors.text_primary);
    pal.setColor(QPalette::Mid, colors.border);
    pal.setColor(QPalette::Dark, colors.bg_deep);
    pal.setColor(QPalette::Light, colors.hover);

    for (const auto role : {QPalette::Text,
                            QPalette::WindowText,
                            QPalette::ButtonText,
                            QPalette::HighlightedText})
      pal.setColor(QPalette::Disabled, role, colors.text_disabled);

    app.setPalette(pal);
  }

  app.setStyleSheet(style_sheet.c_str());

  // re-tint icons to match the (now loaded) palette text color
  ctx.app_settings.icons.apply_text_color(ctx.app_settings.colors.text_primary);

  // graph viewer style
  {
    gngui::Style *p_style = gngui::Style::get_style().get();

    p_style->viewer.color_bg = ctx.app_settings.colors.bg_primary;
    p_style->viewer.disable_during_update = ctx.app_settings.node_editor
                                                .disable_during_update;
  }
}

} // namespace hesiod
