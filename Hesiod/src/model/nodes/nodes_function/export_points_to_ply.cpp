/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/export.hpp"
#include "highmap/geometry/cloud.hpp"
#include "highmap/operator.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/node_factory.hpp"
#include "hesiod/model/nodes/post_process.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *A_PATTERN     = "pattern";
constexpr const char *A_AUTO_EXPORT = "auto_export";
constexpr const char *A_FNAME       = "fname";
constexpr const char *A_LABEL1      = "label1";
constexpr const char *A_LABEL2      = "label2";
constexpr const char *A_LABEL3      = "label3";
constexpr const char *A_XMAX        = "xmax";
constexpr const char *A_XMIN        = "xmin";
constexpr const char *A_YMAX        = "ymax";
constexpr const char *A_YMIN        = "ymin";
constexpr const char *A_ZMAX        = "zmax";
constexpr const char *A_ZMIN        = "zmin";

void setup_export_points_to_ply_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<std::vector<float>>(gnode::PortType::IN, "x");
  node.add_port<std::vector<float>>(gnode::PortType::IN, "y");
  node.add_port<std::vector<float>>(gnode::PortType::IN, "z");
  node.add_port<std::vector<float>>(gnode::PortType::IN, "point_data1");
  node.add_port<std::vector<float>>(gnode::PortType::IN, "point_data2");
  node.add_port<std::vector<float>>(gnode::PortType::IN, "point_data3");

  // attribute(s)
  add_filename(node,
               A_FNAME,
               "fname",
               std::filesystem::path("points.ply"),
               "Stanford PLY (*.ply)",
               true);
  add_string(node, A_PATTERN, "Filename Pattern", "{FILENAME}.{EXT}");
  add_bool(node, A_AUTO_EXPORT, "Auto Export on Node Update", false);
  add_string(node, A_LABEL1, "", "data1");
  add_string(node, A_LABEL2, "", "data2");
  add_string(node, A_LABEL3, "", "data3");
  add_float(node, A_XMIN, "xmin", 0.f, -FLT_MAX, FLT_MAX);
  add_float(node, A_XMAX, "xmax", 1.f, -FLT_MAX, FLT_MAX);
  add_float(node, A_YMIN, "ymin", 0.f, -FLT_MAX, FLT_MAX);
  add_float(node, A_YMAX, "ymax", 1.f, -FLT_MAX, FLT_MAX);
  add_float(node, A_ZMIN, "zmin", 0.f, -FLT_MAX, FLT_MAX);
  add_float(node, A_ZMAX, "zmax", 1.f, -FLT_MAX, FLT_MAX);

  // specialized GUI
}

void compute_export_points_to_ply_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto p_x = node.get_value_ref<std::vector<float>>("x");
  auto p_y = node.get_value_ref<std::vector<float>>("y");
  auto p_z = node.get_value_ref<std::vector<float>>("z");

  if (p_x && p_y && p_z && node.val<bool>(A_AUTO_EXPORT))
  {
    std::filesystem::path fname = node.val<std::filesystem::path>(A_FNAME);
    fname                       = ensure_extension(fname, ".ply");
    const auto pattern          = node.val<std::string>(A_PATTERN);

    std::string ext = fname.extension().string();
    if (!ext.empty() && ext[0] == '.')
      ext = ext.substr(1);

    std::string filename_val = fname.stem().string();
    std::string project_name = HSD_CTX.project_model->get_name();
    std::string width_val    = std::to_string(node.cfg().shape.x);
    std::string height_val   = std::to_string(node.cfg().shape.y);

    std::unordered_map<std::string, std::string> replacements = {
        {"{EXT}", ext},
        {"{WIDTH}", width_val},
        {"{HEIGHT}", height_val},
        {"{PROJECT}", project_name},
        {"{FILENAME}", filename_val}};

    std::filesystem::path export_path =
        make_unique_filename(fname.parent_path(), pattern, replacements);

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
        custom_fields[node.val<std::string>(labels[k])] = *data_ptrs[k];
    }

    // --- export

    auto xr = hmap::rescaled_vector(*p_x,
                                    node.val<float>(A_XMIN),
                                    node.val<float>(A_XMAX));
    auto yr = hmap::rescaled_vector(*p_y,
                                    node.val<float>(A_YMIN),
                                    node.val<float>(A_YMAX));
    auto zr = hmap::rescaled_vector(*p_z,
                                    node.val<float>(A_ZMIN),
                                    node.val<float>(A_ZMAX));

    hmap::export_points_to_ply(export_path.string(), xr, yr, zr, custom_fields);
  }
}

} // namespace hesiod
