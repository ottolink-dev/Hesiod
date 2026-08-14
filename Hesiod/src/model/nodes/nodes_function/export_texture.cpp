/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/export.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

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
constexpr const char *P_TEXTURE = "texture";

constexpr const char *A_16_BIT      = "16 bit";
constexpr const char *A_PATTERN     = "pattern";
constexpr const char *A_AUTO_EXPORT = "auto_export";
constexpr const char *A_FNAME       = "fname";

void setup_export_texture_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_TEXTURE);

  // attribute(s)

  // clang-format off
  add_filename(node, A_FNAME, "fname", std::filesystem::path("texture.png"), "PNG (*.png)", true);
  add_string(node, A_PATTERN, "Filename Pattern", "{FILENAME}.{EXT}");
  add_bool(node, A_16_BIT, "16 bit", false);
  add_bool(node, A_AUTO_EXPORT, "Auto Export on Node Update", false);
  // clang-format on
}

void compute_export_texture_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualTexture *p_in = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE);

  if (p_in && node.val<bool>(A_AUTO_EXPORT))
  {
    std::filesystem::path fname = node.val<std::filesystem::path>(A_FNAME);
    fname                       = ensure_extension(fname, ".png");
    const auto pattern          = node.val<std::string>(A_PATTERN);

    std::unordered_map<std::string, std::string> replacements = get_standard_replacements(
        node,
        fname);

    std::filesystem::path export_path = make_unique_filename(fname.parent_path(),
                                                             pattern,
                                                             replacements);

    if (node.val<bool>(A_16_BIT))
      p_in->to_png(export_path.string(), node.cfg().cm_cpu, CV_16U);
    else
      p_in->to_png(export_path.string(), node.cfg().cm_cpu, CV_8U);
  }
}

} // namespace hesiod
