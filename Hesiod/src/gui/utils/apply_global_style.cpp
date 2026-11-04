/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <map>

#include <QColor>
#include <QPalette>

#include "gnodegui/graph_viewer.hpp"
#include "gnodegui/style.hpp"
#include "meta_qt/ui/theme.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/theme/theme.hpp"
#include "hesiod/gui/theme/theme_registry.hpp"
#include "hesiod/gui/widgets/chrome/window_chrome.hpp"
#include "hesiod/gui/widgets/graph/graph_node_widget.hpp"
#include "hesiod/gui/widgets/graph/graph_tab_strip.hpp"
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

  // Resolve theme from registry based on theme_name
  std::string theme_name = ctx.app_settings.interface.theme_name;
  if (!ThemeRegistry::instance().has_theme(theme_name))
  {
    theme_name = "Dark";
    ctx.app_settings.interface.theme_name = theme_name;
  }

  ThemeDefinition active_theme = ThemeRegistry::instance().get_theme(theme_name);
  ctx.app_settings.colors = active_theme.derive_app_colors();
  ctx.app_settings.node_palette = active_theme.derive_node_palette_style();

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

  // update all active graph viewers and tab strips
  for (QWidget *top : QApplication::topLevelWidgets())
  {
    for (GraphNodeWidget *gnw : top->findChildren<GraphNodeWidget *>())
      gnw->update_style();

    for (GraphTabStrip *strip : top->findChildren<GraphTabStrip *>())
      strip->update();
  }
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
  p_style->node.color_port_data = ctx.node_graph_palette.data_color_map;
  p_style->node.color_category = ctx.node_graph_palette.category_color_map;
  p_style->node.port_radius = ctx.app_settings.node_editor.port_radius;
  p_style->node.show_compute_time = ctx.app_settings.node_editor.show_compute_time;

  ThemeDefinition active_theme = ThemeRegistry::instance().get_theme(
      ctx.app_settings.interface.theme_name);
  p_style->viewer.color_bg = active_theme.node_editor_bg;
  p_style->viewer.disable_during_update = ctx.app_settings.node_editor
                                              .disable_during_update;

  const auto &design = properties_panel_design();
  if (design.has_own_chrome && design.theme)
  {
    const auto &theme = *design.theme;
    auto       &node = p_style->node;
    auto       &viewer = p_style->viewer;
    auto       &link = p_style->link;

    node.color_category = ctx.node_graph_palette.category_color_map;
    node.color_bg = theme.section_surface;
    node.color_bg_light = theme.section_header;

    // Node border: in dark mode use theme.hairline; in light mode ensure a crisp dark
    // border
    if (active_theme.is_dark)
    {
      node.color_border = theme.hairline;
      node.color_border_hovered = theme.field_border_hover;
    }
    else
    {
      node.color_border = QColor("#5A5D62");
      node.color_border_hovered = theme.ink_primary;
    }

    node.color_caption = theme.ink_section_title;
    node.color_icon = theme.ink_icon;
    node.color_comment = theme.ink_secondary;
    node.color_port_label = theme.ink_primary;
    node.color_selected = theme.accent;
    node.color_pinned = theme.accent;
    node.color_port_hovered = theme.accent;
    node.color_port_selected = theme.accent;
    node.color_port_data_default = theme.ink_secondary;
    node.color_port_not_selectable = theme.ink_locked;

    // In light mode, ensure data port and link colors that were pastel or near-white
    // (such as VirtualArray) have enough contrast against the light node surface and
    // editor background
    auto data_colors = ctx.node_graph_palette.data_color_map;
    if (!active_theme.is_dark)
    {
      for (auto &[_, col] : data_colors)
      {
        if (col.lightness() > 160)
        {
          col = QColor::fromHsl(col.hslHue() >= 0 ? col.hslHue() : 210,
                                std::max(60, col.hslSaturation()),
                                55);
        }
      }
    }
    node.color_port_data = data_colors;

    node.rounding_radius = theme.metrics.section_card_radius;
    node.pen_width = 1.f;
    node.pen_width_hovered = 1.f;

    // Node editor background from theme
    viewer.color_bg = active_theme.node_editor_bg;
    viewer.color_toolbar = theme.ink_icon;
    link.color_default = theme.ink_secondary;
    link.color_selected = theme.accent;
  }
}

} // namespace hesiod
