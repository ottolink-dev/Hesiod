/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/export.hpp"
#include "highmap/texture.hpp"
#include "highmap/transform.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

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
constexpr const char *P_TEXTURE = "texture";

constexpr const char *A_16_BIT      = "16 bit";
constexpr const char *A_ADD_PREFIX  = "add_prefix";
constexpr const char *A_AUTO_EXPORT = "auto_export";
constexpr const char *A_FNAME       = "fname";
constexpr const char *A_FLIP_X      = "flip_x";
constexpr const char *A_FLIP_Y      = "flip_y";

void setup_export_texture_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_TEXTURE);

  // attribute(s)
  add_filename(node,
               A_FNAME,
               "fname",
               std::filesystem::path("texture.png"),
               "PNG (*.png)",
               true);
  add_bool(node, A_16_BIT, "16 bit", false);
  add_bool(node, A_AUTO_EXPORT, "Auto Export on Node Update", false);
  add_bool(node, A_ADD_PREFIX, "Add Project Name as Prefix", false);
  add_bool(node, A_FLIP_X, "Flip-X", false);
  add_bool(node, A_FLIP_Y, "Flip-Y", false);

  // specialized GUI
}

void compute_export_texture_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualTexture *p_in = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE);

  if (p_in && node.val<bool>(A_AUTO_EXPORT))
  {
    std::filesystem::path fname = node.val<std::filesystem::path>(A_FNAME);
    fname                       = ensure_extension(fname, ".png");

    if (node.val<bool>(A_ADD_PREFIX))
      fname = prepend_project_name_to_path(fname);

    const bool flip_x = node.val<bool>(A_FLIP_X);
    const bool flip_y = node.val<bool>(A_FLIP_Y);
    const int  depth  = node.val<bool>(A_16_BIT) ? CV_16U : CV_8U;

    hmap::Texture t = p_in->to_texture(p_in->shape, node.cfg().cm_cpu);
    if (flip_x)
      hmap::flip_lr(t);
    if (flip_y)
      hmap::flip_ud(t);
    t.to_png(fname.string(), depth);
  }
}

} // namespace hesiod
