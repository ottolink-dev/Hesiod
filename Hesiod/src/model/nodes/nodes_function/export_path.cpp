/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/geometry/path.hpp"

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
constexpr const char *P_IN = "input";

constexpr const char *A_PATTERN     = "pattern";
constexpr const char *A_AUTO_EXPORT = "auto_export";
constexpr const char *A_FNAME       = "fname";

void setup_export_path_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Path>(gnode::PortType::IN, P_IN);

  // attribute(s)
  add_filename(node,
               A_FNAME,
               "fname",
               std::filesystem::path("path.csv"),
               "CSV (*.csv)",
               true);
  add_string(node, A_PATTERN, "Filename Pattern", "{FILENAME}.{EXT}");
  add_bool(node, A_AUTO_EXPORT, "Auto Export on Node Update", false);

  // specialized GUI
}

void compute_export_path_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Path *p_in = node.get_value_ref<hmap::Path>(P_IN);

  if (p_in && node.val<bool>(A_AUTO_EXPORT))
  {
    std::filesystem::path fname = node.val<std::filesystem::path>(A_FNAME);
    fname                       = ensure_extension(fname, ".csv");
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

    p_in->to_csv(export_path.string());
  }
}

} // namespace hesiod
