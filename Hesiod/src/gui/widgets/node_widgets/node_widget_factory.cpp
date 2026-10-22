/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/graph_node_widget.hpp"
#include "hesiod/gui/widgets/node_widget.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

void apply_node_status_icons(const std::string      &node_type,
                             std::weak_ptr<BaseNode> model,
                             GraphNodeWidget        *p_gnw,
                             const std::string      &node_id)
{
  if (!p_gnw)
    return;

  auto *gfx_node = p_gnw->get_graphics_node_by_id(node_id);
  if (!gfx_node)
    return;

  std::vector<gngui::GraphicsNode::StatusIcon> icons;

  bool is_wip = false;
  bool is_deprecated = false;

  if (auto m = model.lock())
  {
    std::string cat = m->get_category();

    if (cat.find("WIP") != std::string::npos)
      is_wip = true;

    if (cat.find("DEPRECATED") != std::string::npos)
      is_deprecated = true;
  }

  static const std::vector<std::string> wip_nodes = {"CoastalErosionDiffusion",
                                                     "HydraulicProcedural",
                                                     "HydraulicSaleve",
                                                     "Island",
                                                     "MountainCone",
                                                     "MountainInselberg",
                                                     "MountainStump",
                                                     "MountainTibesti",
                                                     "Strata",
                                                     "Recurve",
                                                     "Rifts",
                                                     "ShatteredPeak",
                                                     "WaterDepthFromMask"};

  if (contains<std::string>(wip_nodes, node_type))
    is_wip = true;

  static const std::vector<std::string> not_tileable_nodes = {"Compare",
                                                              "Debug",
                                                              "DepressionFilling",
                                                              "DistanceTransform",
                                                              "ExportAsCubemap",
                                                              "ExportAsset",
                                                              "ExportHeightmap",
                                                              "ExportScene",
                                                              "ExportTexture",
                                                              "ExportTiled",
                                                              "FloodingFromBoundaries",
                                                              "FloodingFromPoint",
                                                              "FloodingLakeSystem",
                                                              "FlowSimulation",
                                                              "FlowSimulationViscous",
                                                              "FlowStream",
                                                              "HydraulicMise",
                                                              "ImportHeightmap",
                                                              "ImportTexture",
                                                              "KmeansClustering2",
                                                              "KmeansClustering3",
                                                              "MakePeriodic",
                                                              "MakePeriodicStitching",
                                                              "Mudslide",
                                                              "Preview",
                                                              "QuiltingBlend",
                                                              "QuiltingExpand",
                                                              "QuiltingShuffle",
                                                              "RockSimulation",
                                                              "Rotate",
                                                              "SnowSimulation",
                                                              "TerrainTexture",
                                                              "TextureQuiltingExpand",
                                                              "TextureQuiltingShuffle",
                                                              "TextureUvChecker",
                                                              "Translate",
                                                              "Zoom"};

  const bool is_not_tileable = contains<std::string>(not_tileable_nodes, node_type);

  if (is_deprecated)
  {
    icons.push_back(
        {.icon = HSD_ICON("event_busy"), .tooltip = "Deprecated: this node is obsolete"});
  }

  if (is_wip)
  {
    icons.push_back({.icon = HSD_ICON("construction"),
                     .tooltip = "Work in progress: node subject to change"});
  }

  if (is_not_tileable)
  {
    icons.push_back(
        {.icon = HSD_ICON("grid_off"),
         .tooltip = "Not tileable: this node does not support seamless periodic tiling"});
  }

  gfx_node->set_status_icons(icons);
}

QWidget *node_widget_factory(const std::string        &node_type,
                             std::weak_ptr<BaseNode>   model,
                             QPointer<GraphNodeWidget> p_gnw,
                             QWidget                  *parent)
{
  std::string msg = "";

  // create node
  if (node_type.starts_with("Export"))
  {
    return new ExportNodeWidget(model, p_gnw, msg, parent);
  }
  else if (node_type == "Debug")
  {
    return new DebugNodeWidget(model, p_gnw, msg, parent);
  }
  else if (node_type == "Toggle")
  {
    return new ToggleNodeWidget(model, p_gnw, msg, parent);
  }
  else if (node_type == "Thru")
  {
    return new ThruNodeWidget(model, p_gnw, msg, parent);
  }

  // default
  return new NodeWidget(model, p_gnw, msg, parent);
}

} // namespace hesiod
