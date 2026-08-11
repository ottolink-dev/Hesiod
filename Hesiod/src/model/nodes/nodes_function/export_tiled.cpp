/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/export.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/node_factory.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_IN = "input";

constexpr const char *A_ADD_PREFIX              = "add_prefix";
constexpr const char *A_AUTO_EXPORT             = "auto_export";
constexpr const char *A_BIT_DEPTH               = "bit_depth";
constexpr const char *A_FNAME                   = "fname";
constexpr const char *A_LEADING_ZEROS           = "leading_zeros";
constexpr const char *A_OVERLAPPING_EDGES       = "overlapping_edges";
constexpr const char *A_REVERSE_TILE_Y_INDEXING = "reverse_tile_y_indexing";
constexpr const char *A_TILING_X                = "tiling.x";
constexpr const char *A_TILING_Y                = "tiling.y";

void setup_export_tiled_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);

  // attribute(s)
  add_filename(node, A_FNAME, "fname", std::filesystem::path("hmap.png"), "*", true);

  std::vector<std::string> choices = {"8 bit", "16 bit"};
  add_choice(node, A_BIT_DEPTH, "PNG Bit Depth", choices, "16 bit");
  add_int(node, A_TILING_X, "Nb. of Tiles (x)", 4, 1, INT_MAX);
  add_int(node, A_TILING_Y, "Nb. of Tiles (y)", 4, 1, INT_MAX);
  add_int(node, A_LEADING_ZEROS, "Leading Zeroes", 3, 1, 6);
  add_bool(node, A_OVERLAPPING_EDGES, "Overlapping Edges", false);
  add_bool(node, A_REVERSE_TILE_Y_INDEXING, "Reverse y-indexing", false);
  add_bool(node, A_AUTO_EXPORT, "Auto Export on Node Update", false);
  add_bool(node, A_ADD_PREFIX, "Add Project Name as Prefix", false);
}

void compute_export_tiled_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in && node.val<bool>(A_AUTO_EXPORT))
  {
    // prepare parameters
    std::filesystem::path fname = node.val<std::filesystem::path>(A_FNAME);

    if (node.val<bool>(A_ADD_PREFIX))
      fname = prepend_project_name_to_path(fname);

    int bit_depth;
    if (node.val<std::string>(A_BIT_DEPTH) == "8 bit")
      bit_depth = CV_8U;
    else
      bit_depth = CV_16U;

    glm::ivec2 tiling = {node.val<int>(A_TILING_X), node.val<int>(A_TILING_Y)};

    // export
    hmap::export_tiled(fname.string(),
                       "png",
                       p_in->to_array(node.cfg().cm_cpu),
                       tiling,
                       node.val<int>(A_LEADING_ZEROS),
                       bit_depth,
                       node.val<bool>(A_OVERLAPPING_EDGES),
                       node.val<bool>(A_REVERSE_TILE_Y_INDEXING));
  }
}

} // namespace hesiod
