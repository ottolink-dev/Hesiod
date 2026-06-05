/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "gnodegui/style.hpp"

#include "qsx/config.hpp"

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

  // QSliderX style
  {
    qsx::Config *p_cfg = qsx::Config::get_config().get();

    p_cfg->global.color_text = ctx.app_settings.colors.text_primary;
    p_cfg->global.color_border = ctx.app_settings.colors.border;
    p_cfg->global.color_hovered = ctx.app_settings.colors.hover;
    p_cfg->global.color_selected = ctx.app_settings.colors.accent;
    p_cfg->global.color_faded = ctx.app_settings.colors.text_disabled;
    p_cfg->global.color_bg = ctx.app_settings.colors.bg_secondary;
  }
}

} // namespace hesiod
