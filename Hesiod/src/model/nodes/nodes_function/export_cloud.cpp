/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/geometry/cloud.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/node_factory.hpp"
#include "hesiod/model/nodes/post_process.hpp"
#include "hesiod/model/utils.hpp"

using namespace attr;

namespace hesiod
{

void setup_export_cloud_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Cloud>(gnode::PortType::IN, "input");

  // attribute(s)
  node.add_attr<FilenameAttribute>("fname",
                                   "fname",
                                   std::filesystem::path("cloud.csv"),
                                   "CSV (*.csv)",
                                   true);
  node.add_attr<BoolAttribute>("auto_export", "Auto Export on Node Update", false);
  node.add_attr<BoolAttribute>("add_prefix", "Add Project Name as Prefix", false);

  // attribute(s) order
  node.set_attr_ordered_key({"fname", "auto_export", "add_prefix"});

  // specialized GUI
}

void compute_export_cloud_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Cloud *p_in = node.get_value_ref<hmap::Cloud>("input");

  if (p_in && node.get_attr<BoolAttribute>("auto_export"))
  {
    std::filesystem::path fname = node.get_attr<FilenameAttribute>("fname");
    fname                       = ensure_extension(fname, ".csv");

    if (node.get_attr<BoolAttribute>("add_prefix"))
      fname = prepend_project_name_to_path(fname);

    p_in->to_csv(fname.string());
  }
}

} // namespace hesiod
