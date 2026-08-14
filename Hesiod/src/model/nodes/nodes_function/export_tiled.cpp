/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/export.hpp"
#include "highmap/transform.hpp"

#include "hesiod/app/hesiod_application.hpp"
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

constexpr const char *A_PATTERN                 = "pattern";
constexpr const char *A_AUTO_EXPORT             = "auto_export";
constexpr const char *A_BIT_DEPTH               = "bit_depth";
constexpr const char *A_FNAME                   = "fname";
constexpr const char *A_LEADING_ZEROS           = "leading_zeros";
constexpr const char *A_OVERLAPPING_EDGES       = "overlapping_edges";
constexpr const char *A_REVERSE_TILE_Y_INDEXING = "reverse_tile_y_indexing";
constexpr const char *A_TILING_X                = "tiling.x";
constexpr const char *A_TILING_Y                = "tiling.y";
constexpr const char *A_FLIP_X                  = "flip_x";
constexpr const char *A_FLIP_Y                  = "flip_y";

void setup_export_tiled_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);

  // attribute(s)
  add_filename(node, A_FNAME, "fname", std::filesystem::path("hmap.png"), "*", true);
  add_string(node, A_PATTERN, "Filename Pattern", "{FILENAME}.{EXT}");

  std::vector<std::string> choices = {"8 bit", "16 bit"};
  add_choice(node, A_BIT_DEPTH, "PNG Bit Depth", choices, "16 bit");
  add_int(node, A_TILING_X, "Nb. of Tiles (x)", 4, 1, INT_MAX);
  add_int(node, A_TILING_Y, "Nb. of Tiles (y)", 4, 1, INT_MAX);
  add_int(node, A_LEADING_ZEROS, "Leading Zeroes", 3, 1, 6);
  add_bool(node, A_OVERLAPPING_EDGES, "Overlapping Edges", false);
  add_bool(node, A_REVERSE_TILE_Y_INDEXING, "Reverse y-indexing", false);
  add_bool(node, A_AUTO_EXPORT, "Auto Export on Node Update", false);
  add_bool(node, A_FLIP_X, "Flip-X", false);
  add_bool(node, A_FLIP_Y, "Flip-Y", false);
}

void compute_export_tiled_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in && node.val<bool>(A_AUTO_EXPORT))
  {
    // prepare parameters
    std::filesystem::path fname   = node.val<std::filesystem::path>(A_FNAME);
    const auto            pattern = node.val<std::string>(A_PATTERN);

    std::unordered_map<std::string, std::string> replacements = get_standard_replacements(
        node,
        fname);

    std::filesystem::path export_path = make_unique_filename(fname.parent_path(),
                                                             pattern,
                                                             replacements);

    int bit_depth;
    if (node.val<std::string>(A_BIT_DEPTH) == "8 bit")
      bit_depth = CV_8U;
    else
      bit_depth = CV_16U;

    glm::ivec2 tiling = {node.val<int>(A_TILING_X), node.val<int>(A_TILING_Y)};

    hmap::Array array  = p_in->to_array(node.cfg().cm_cpu);
    const bool  flip_x = node.val<bool>(A_FLIP_X);
    const bool  flip_y = node.val<bool>(A_FLIP_Y);

    if (flip_x)
      hmap::flip_lr(array);
    if (flip_y)
      hmap::flip_ud(array);

    // export
    hmap::export_tiled(export_path.string(),
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
