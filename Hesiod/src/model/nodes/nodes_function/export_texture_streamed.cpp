/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/export.hpp"
#include "highmap/transform.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/node_factory.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_TEXTURE = "texture";

constexpr const char *A_FILENAME    = "fname";
constexpr const char *A_PATTERN     = "pattern";
constexpr const char *A_FORMAT      = "format";
constexpr const char *A_AUTO_EXPORT = "auto_export";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_export_texture_streamed_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_TEXTURE);

  // --- Attributes

  std::vector<std::string> format_choices = {"OpenEXR (32-bit float) - *.exr",
                                             "BigTIFF (32-bit float) - *.tif"};

  // clang-format off
  node.set_current_category("Filename");
  add_filename(node, A_FILENAME, "Filename", "texture.tif", "Images (*.exr *.tif *.tiff *.btif *.bigtiff)", true);
  add_string(node, A_PATTERN, "Filename Pattern", "{FILENAME}.{EXT}");

  node.set_current_category("Export Parameters");
  add_choice(node, A_FORMAT, "File Format", format_choices, "BigTIFF (32-bit float) - *.tif");
  add_bool(node, A_AUTO_EXPORT, "Auto Export on Node Update", false);
  // clang-format on
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_export_texture_streamed_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs

  auto *p_in = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE);

  if (!p_in)
    return;

  // --- Params

  const auto auto_export = node.val<bool>(A_AUTO_EXPORT);
  auto       fname       = node.val<std::filesystem::path>(A_FILENAME);
  const auto pattern     = node.val<std::string>(A_PATTERN);
  const auto format_str  = node.val<std::string>(A_FORMAT);

  if (!auto_export)
  {
    Logger::log()->trace(
        "compute_export_texture_streamed_node: [{}]/[{}]: auto export is disabled",
        node.get_node_type(),
        node.get_id());
    return;
  }

  // --- Prepare filename

  std::string default_ext = ".tif";

  if (format_str.find("OpenEXR") != std::string::npos ||
      format_str.find(".exr") != std::string::npos)
  {
    default_ext = ".exr";
  }

  fname = ensure_extension(fname, default_ext);

  // --- Export using make_unique_filename

  std::unordered_map<std::string, std::string> replacements = get_standard_replacements(
      node,
      fname);

  std::filesystem::path export_path = make_unique_filename(fname.parent_path(),
                                                           pattern,
                                                           replacements);

  Logger::log()->trace(
      "compute_export_texture_streamed_node: [{}]/[{}]: export path = {}",
      node.get_node_type(),
      node.get_id(),
      export_path.string());

  // --- Stream export using export_virtual_array

  hmap::ImageWriterConfig config;
  config.image_shape = p_in->shape;
  config.tile_shape  = p_in->tile_shape;
  config.channels    = p_in->channels();
  config.data_type   = hmap::ImageDataType::FLOAT32;

  std::unique_ptr<hmap::ImageWriter> writer = hmap::ImageWriter::create(
      export_path.string(),
      config);
  if (!writer || !writer->open(export_path.string(), config))
  {
    Logger::log()->error(
        "compute_export_texture_streamed_node: failed to open writer for {}",
        export_path.string());
    return;
  }

  hmap::export_virtual_array(*p_in, *writer, node.cfg().cm_cpu);
}

} // namespace hesiod
