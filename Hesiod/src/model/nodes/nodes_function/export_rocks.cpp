/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/rocks/rock_field.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
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

constexpr const char *G_CSV = "csv";
constexpr const char *G_PLY = "ply";

constexpr const char *A_PATTERN     = "pattern";
constexpr const char *A_AUTO_EXPORT = "auto_export";
constexpr const char *A_FNAME       = "fname";

void setup_export_rocks_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::RockField>(gnode::PortType::IN, P_IN);

  // Group: csv
  {
    node.set_current_group(G_CSV);

    // clang-format off
    add_filename(node, A_FNAME, "fname", std::filesystem::path("rocks.csv"), "CSV (*.csv)", true);
    add_string(node, A_PATTERN, "Filename Pattern", "{FILENAME}.{EXT}");
    add_bool(node, A_AUTO_EXPORT, "Auto Export on Node Update", false);
    // clang-format on
  }

  // Group: ply
  {
    node.set_current_group(G_PLY);

    // clang-format off
    add_filename(node, A_FNAME, "fname", std::filesystem::path("rocks.ply"), "Stanford PLY (*.ply)", true);
    add_string(node, A_PATTERN, "Filename Pattern", "{FILENAME}.{EXT}");
    add_bool(node, A_AUTO_EXPORT, "Auto Export on Node Update", false);
    // clang-format on
  }

  // Reset active group to first
  node.set_current_group(G_CSV);
}

void compute_export_rocks_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::RockField *p_in = node.get_value_ref<hmap::RockField>(P_IN);

  if (!p_in)
    return;

  const bool auto_export = node.val<bool>(A_AUTO_EXPORT);
  if (!auto_export)
  {
    Logger::log()->trace("compute_export_rocks_node: [{}]/[{}]: auto export is disabled",
                         node.get_node_type(),
                         node.get_id());
    return;
  }

  const std::string group = node.get_meta_group().current_container_name().value_or(
      G_CSV);

  const std::string     target_ext = (group == G_PLY) ? ".ply" : ".csv";
  std::filesystem::path fname      = node.val<std::filesystem::path>(A_FNAME);
  fname                            = ensure_extension(fname, target_ext);
  const auto pattern               = node.val<std::string>(A_PATTERN);

  std::unordered_map<std::string, std::string> replacements = get_standard_replacements(
      node,
      fname);

  std::filesystem::path export_path = make_unique_filename(fname.parent_path(),
                                                           pattern,
                                                           replacements);

  Logger::log()->trace("compute_export_rocks_node: [{}]/[{}]: export path = {}",
                       node.get_node_type(),
                       node.get_id(),
                       export_path.string());

  if (group == G_PLY)
  {
    p_in->to_ply(export_path.string());
  }
  else
  {
    p_in->to_csv(export_path.string());
  }
}

} // namespace hesiod
