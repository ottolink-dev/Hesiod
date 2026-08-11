/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/export.hpp"

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

constexpr const char *A_16BIT       = "16bit";
constexpr const char *A_ADD_PREFIX  = "add_prefix";
constexpr const char *A_AUTO_EXPORT = "auto_export";
constexpr const char *A_FNAME       = "fname";

void setup_export_normal_map_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);

  // attribute(s)
  add_filename(node,
               A_FNAME,
               "fname",
               std::filesystem::path("nmap.png"),
               "PNG (*.png)",
               true);
  add_bool(node, A_16BIT, "16bit", false);
  add_bool(node, A_AUTO_EXPORT, "Auto Export on Node Update", false);
  add_bool(node, A_ADD_PREFIX, "Add Project Name as Prefix", false);

  // specialized GUI
}

void compute_export_normal_map_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in && node.val<bool>(A_AUTO_EXPORT))
  {
    std::filesystem::path fname = node.val<std::filesystem::path>(A_FNAME);
    fname                       = ensure_extension(fname, ".png");

    if (node.val<bool>(A_ADD_PREFIX))
      fname = prepend_project_name_to_path(fname);

    if (node.val<bool>(A_16BIT))
      hmap::export_normal_map_png(fname.string(),
                                  p_in->to_array(node.cfg().cm_cpu),
                                  CV_16U);
    else
      hmap::export_normal_map_png(fname.string(),
                                  p_in->to_array(node.cfg().cm_cpu),
                                  CV_8U);
  }
}

} // namespace hesiod
