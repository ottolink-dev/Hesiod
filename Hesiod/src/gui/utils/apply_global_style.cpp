/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <map>

#include <QColor>
#include <QPalette>

#include "gnodegui/style.hpp"
#include "meta_qt/ui/theme.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/chrome/window_chrome.hpp"
#include "hesiod/gui/widgets/node_widgets/node_palette_sidebar.hpp"
#include "hesiod/gui/widgets/node_widgets/node_settings_widget.hpp"
#include "hesiod/gui/widgets/node_widgets/properties_panel_design.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

void apply_global_style(QApplication &app)
{
  Logger::log()->trace("apply_global_style");

  AppContext &ctx = HSD_CTX;

  // Sync palette colors with current dark/light mode setting
  if (ctx.app_settings.interface.dark_mode)
  {
    ctx.app_settings.colors = AppSettings::Colors::make_dark();
    ctx.app_settings.node_palette = NodePaletteStyle::make_dark();
  }
  else
  {
    ctx.app_settings.colors = AppSettings::Colors::make_light();
    ctx.app_settings.node_palette = NodePaletteStyle::make_light();
  }

  std::string style_sheet =
#include "hesiod/gui/darkstyle.css"
      ;

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
      {"COLOR_SEPARATOR", ctx.app_settings.colors.separator},
      // derived tones for the panel cards and popups: the configured border and
      // accent are too loud at the size of a whole pane or a menu highlight
      {"COLOR_PANEL_BORDER", panel_border_color()},
      {"COLOR_PANEL_HOVER",
       mix_colors(ctx.app_settings.colors.bg_deep,
                  ctx.app_settings.colors.bg_primary,
                  0.6)},
      {"COLOR_MUTED_ACCENT",
       mix_colors(ctx.app_settings.colors.bg_primary,
                  ctx.app_settings.colors.accent,
                  0.55)}};

  for (auto &[p, color] : place_holders)
    hesiod::replace_all(style_sheet, p, color.name().toStdString());

  if (ctx.app_settings.icons.icon_paths.contains("check"))
    hesiod::replace_all(style_sheet,
                        "ICON_CHECK_PATH",
                        ctx.app_settings.icons.icon_paths.at("check"));

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

    // Update MetaUI theme registry so Theme::from_palette() picks up the new palette
    meta::qt::ThemeRegistry::instance().update_palette_theme(pal);
  }

  app.setStyleSheet(style_sheet.c_str());

  // re-tint icons to match the (now loaded) palette text color
  ctx.app_settings.icons.apply_text_color(ctx.app_settings.colors.text_primary);

  // update title bars and menu bars
  for (QWidget *top : QApplication::topLevelWidgets())
  {
    for (TitleBar *bar : top->findChildren<TitleBar *>())
      bar->update_style();
  }

  // update node settings widgets
  NodeSettingsWidget::restyle_all();

  // update node palette sidebars
  NodePaletteSidebar::restyle_all(current_node_palette_style());

  // synchronize graph viewer and node styles
  apply_graph_style();
}

void apply_graph_style()
{
  Logger::log()->trace("apply_graph_style");

  AppContext   &ctx = HSD_CTX;
  gngui::Style *p_style = gngui::Style::get_style().get();

  p_style->viewer.add_toolbar = false;
  p_style->viewer.add_new_icon = false;
  p_style->viewer.add_load_save_icons = false;
  p_style->viewer.add_group = ctx.app_settings.node_editor.enable_node_groups;
  p_style->node.color_port_data = ctx.style_settings.data_color_map;
  p_style->node.color_category = ctx.style_settings.category_color_map;
  p_style->node.port_radius = ctx.app_settings.node_editor.port_radius;
  p_style->node.show_compute_time = ctx.app_settings.node_editor.show_compute_time;
  // Keep a dark background for the node editor background, even in light mode
  p_style->viewer.color_bg = QColor("#2B2B2B");
  p_style->viewer.disable_during_update = ctx.app_settings.node_editor
                                              .disable_during_update;

  constexpr qreal kCategoryTint = 0.22;

  const auto &design = properties_panel_design();
  if (design.has_own_chrome && design.theme)
  {
    const auto &theme = *design.theme;
    auto       &node = p_style->node;
    auto       &viewer = p_style->viewer;
    auto       &link = p_style->link;

    auto tint_towards = [](const QColor &surface, const QColor &accent, qreal t)
    {
      return QColor::fromRgbF(surface.redF() * (1.0 - t) + accent.redF() * t,
                              surface.greenF() * (1.0 - t) + accent.greenF() * t,
                              surface.blueF() * (1.0 - t) + accent.blueF() * t);
    };

    std::map<std::string, QColor> tinted_categories;
    for (const auto &[category, color] : ctx.style_settings.category_color_map)
      tinted_categories[category] = tint_towards(theme.section_header,
                                                 color,
                                                 kCategoryTint);

    node.color_category = tinted_categories;
    node.color_bg = theme.section_surface;
    node.color_bg_light = theme.section_header;
    node.color_border = theme.hairline;
    node.color_border_hovered = theme.field_border_hover;
    node.color_caption = theme.ink_section_title;
    node.color_icon = theme.ink_icon;
    node.color_comment = theme.ink_secondary;
    node.color_selected = theme.accent;
    node.color_pinned = theme.accent;
    node.color_port_hovered = theme.accent;
    node.color_port_selected = theme.accent;
    node.color_port_data_default = theme.ink_secondary;
    node.color_port_not_selectable = theme.ink_locked;

    node.rounding_radius = theme.metrics.section_card_radius;
    node.pen_width = 1.f;
    node.pen_width_hovered = 1.f;

    // Keep a dark background for the node editor background, even in light mode
    viewer.color_bg = QColor("#2B2B2B");
    viewer.color_toolbar = theme.ink_icon;
    link.color_default = theme.ink_secondary;
    link.color_selected = theme.accent;
  }
}

} // namespace hesiod
