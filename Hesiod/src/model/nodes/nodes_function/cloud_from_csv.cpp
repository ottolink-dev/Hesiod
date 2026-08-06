/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/geometry/cloud.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"

using namespace attr;

namespace hesiod
{

void setup_cloud_from_csv_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Cloud>(gnode::PortType::OUT, "cloud");

  // attribute(s)
  node.add_attr<FilenameAttribute>("fname",
                                   "fname",
                                   std::filesystem::path(""),
                                   "CSV files (*.csv)",
                                   false);

  // attribute(s) order
  node.set_attr_ordered_key({"fname"});
}

void compute_cloud_from_csv_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Cloud *p_out = node.get_value_ref<hmap::Cloud>("cloud");

  const std::string fname = node.get_attr<FilenameAttribute>("fname").string();
  std::ifstream     f(fname.c_str());

  if (f.good())
  {
    hmap::Cloud cloud_tmp;
    bool        ret = cloud_tmp.from_csv(fname);
    if (ret)
      *p_out = cloud_tmp;
    else
      Logger::log()->error("compute_cloud_from_csv_node: could not parse CSV file {}",
                           fname);
  }
  else
  {
    Logger::log()->error("compute_cloud_from_csv_node: could not load CSV file {}",
                         fname);
  }
}

} // namespace hesiod
