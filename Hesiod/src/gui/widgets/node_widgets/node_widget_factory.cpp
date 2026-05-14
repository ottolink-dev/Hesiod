/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/gui/widgets/node_widget.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

QWidget *node_widget_factory(const std::string        &node_type,
                             std::weak_ptr<BaseNode>   model,
                             QPointer<GraphNodeWidget> p_gnw,
                             QWidget                  *parent)
{
  std::string msg = "";

  // automatic msg for WIP and DEPRECATED categories
  if (auto m = model.lock())
  {
    std::string cat = m->get_category();

    if (cat.find("WIP") != std::string::npos)
      msg = "Work in progress\nNode subject to change";

    if (cat.find("DEPRECATED") != std::string::npos)
      msg = "!!! Deprecated !!!";
  }

  // manually adjust msg for some
  std::vector<std::string> wip_nodes = {"CoastalErosionDiffusion",
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
    msg = "Work in progress\nNode subject to change";

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
