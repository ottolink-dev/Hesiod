/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/export.hpp"

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

constexpr const char *A_PATTERN            = "pattern";
constexpr const char *A_AUTO_EXPORT        = "auto_export";
constexpr const char *A_CUBEMAP_RESOLUTION = "cubemap_resolution";
constexpr const char *A_FNAME              = "fname";
constexpr const char *A_IR                 = "ir";
constexpr const char *A_OVERLAP            = "overlap";
constexpr const char *A_SPLITTED           = "splitted";

void setup_export_as_cubemap_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);

  // attribute(s)
  add_filename(node,
               A_FNAME,
               "fname",
               std::filesystem::path("cubemap.png"),
               "PNG (*.png)",
               true);
  add_string(node, A_PATTERN, "Filename Pattern", "{FILENAME}.{EXT}");
  add_int(node, A_CUBEMAP_RESOLUTION, "cubemap_resolution", 64, 32, INT_MAX);
  add_float(node, A_OVERLAP, "overlap", 0.25f, 0.1f, 5.f);
  add_int(node, A_IR, "ir", 16, 1, INT_MAX);
  add_bool(node, A_SPLITTED, "splitted", false);
  add_bool(node, A_AUTO_EXPORT, "Auto Export on Node Update", false);

  // specialized GUI
}

void compute_export_as_cubemap_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in && node.val<bool>(A_AUTO_EXPORT))
  {
    hmap::Array z = p_in->to_array(node.cfg().cm_cpu);

    std::filesystem::path fname = node.val<std::filesystem::path>(A_FNAME);
    fname                       = ensure_extension(fname, ".png");
    const auto pattern          = node.val<std::string>(A_PATTERN);

    std::unordered_map<std::string, std::string> replacements = get_standard_replacements(
        node,
        fname);

    std::filesystem::path export_path = make_unique_filename(fname.parent_path(),
                                                             pattern,
                                                             replacements);

    hmap::export_as_cubemap(export_path.string(),
                            z,
                            node.val<int>(A_CUBEMAP_RESOLUTION),
                            node.val<float>(A_OVERLAP),
                            node.val<int>(A_IR),
                            hmap::Cmap::GRAY,
                            node.val<bool>(A_SPLITTED),
                            nullptr);
  }
}

} // namespace hesiod
