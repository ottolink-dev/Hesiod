/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/export.hpp"
#include "highmap/geometry/cloud.hpp"
#include "highmap/operator.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/node_factory.hpp"
#include "hesiod/model/nodes/post_process.hpp"
#include "hesiod/model/utils.hpp"

using namespace attr;

namespace hesiod
{

void setup_export_cloud_to_ply_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Cloud>(gnode::PortType::IN, "cloud");
  node.add_port<std::vector<float>>(gnode::PortType::IN, "point_data1");
  node.add_port<std::vector<float>>(gnode::PortType::IN, "point_data2");
  node.add_port<std::vector<float>>(gnode::PortType::IN, "point_data3");

  // attribute(s)
  node.add_attr<FilenameAttribute>("fname",
                                   "fname",
                                   std::filesystem::path("points.ply"),
                                   "Stanford PLY (*.ply)",
                                   true);
  node.add_attr<BoolAttribute>("auto_export", "Auto Export on Node Update", false);
  node.add_attr<BoolAttribute>("add_prefix", "Add Project Name as Prefix", false);
  node.add_attr<StringAttribute>("label1", "", "data1");
  node.add_attr<StringAttribute>("label2", "", "data2");
  node.add_attr<StringAttribute>("label3", "", "data3");
  node.add_attr<FloatAttribute>("xmin", "xmin", 0.f, -FLT_MAX, FLT_MAX);
  node.add_attr<FloatAttribute>("xmax", "xmax", 1.f, -FLT_MAX, FLT_MAX);
  node.add_attr<FloatAttribute>("ymin", "ymin", 0.f, -FLT_MAX, FLT_MAX);
  node.add_attr<FloatAttribute>("ymax", "ymax", 1.f, -FLT_MAX, FLT_MAX);
  node.add_attr<FloatAttribute>("zmin", "zmin", 0.f, -FLT_MAX, FLT_MAX);
  node.add_attr<FloatAttribute>("zmax", "zmax", 1.f, -FLT_MAX, FLT_MAX);

  // attribute(s) order
  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Main Parameters",
                             "fname",
                             "auto_export",
                             "add_prefix",
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Custom data labels",
                             "label1",
                             "label2",
                             "label3",
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Rescaling",
                             "xmin",
                             "xmax",
                             "ymin",
                             "ymax",
                             "zmin",
                             "zmax",
                             "_GROUPBOX_END_"});

  // specialized GUI
}

void compute_export_cloud_to_ply_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Cloud *p_in = node.get_value_ref<hmap::Cloud>("cloud");

  if (p_in && node.get_attr<BoolAttribute>("auto_export"))
  {
    std::filesystem::path fname = node.get_attr<FilenameAttribute>("fname");
    fname = ensure_extension(fname, ".ply");

    if (node.get_attr<BoolAttribute>("add_prefix"))
      fname = prepend_project_name_to_path(fname);

    // --- create custom fields

    std::map<std::string, std::vector<float>> custom_fields = {};

    std::vector<std::string> labels = {"label1", "label2", "label3"};

    std::vector<std::vector<float> *> data_ptrs = {
        node.get_value_ref<std::vector<float>>("point_data1"),
        node.get_value_ref<std::vector<float>>("point_data2"),
        node.get_value_ref<std::vector<float>>("point_data3")};

    for (size_t k = 0; k < labels.size(); ++k)
    {
      if (data_ptrs[k])
        custom_fields[node.get_attr<StringAttribute>(labels[k])] = *data_ptrs[k];
    }

    // --- export

    auto xr = hmap::rescaled_vector(p_in->get_x(),
                                    node.get_attr<FloatAttribute>("xmin"),
                                    node.get_attr<FloatAttribute>("xmax"));
    auto yr = hmap::rescaled_vector(p_in->get_y(),
                                    node.get_attr<FloatAttribute>("ymin"),
                                    node.get_attr<FloatAttribute>("ymax"));
    auto zr = hmap::rescaled_vector(p_in->get_values(),
                                    node.get_attr<FloatAttribute>("zmin"),
                                    node.get_attr<FloatAttribute>("zmax"));

    hmap::export_points_to_ply(fname.string(), xr, yr, zr, custom_fields);
  }
}

} // namespace hesiod
