/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/export.hpp"

#include "attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/node_factory.hpp"
#include "hesiod/model/utils.hpp"

using namespace attr;

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN = "input";

constexpr const char *A_FILENAME = "fname";
constexpr const char *A_FORMAT = "format";
constexpr const char *A_AUTO_EXPORT = "auto_export";
constexpr const char *A_ADD_PREFIX = "add_prefix";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_export_heightmap_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);

  // --- Attributes

  // clang-format off
  node.add_attr<FilenameAttribute>(A_FILENAME, "Filename", std::filesystem::path("hmap.png"), "*", true);
  node.add_attr<EnumAttribute>(A_FORMAT, "File Format", enum_mappings.heightmap_export_format_map, "png (16 bit)");
  node.add_attr<BoolAttribute>(A_AUTO_EXPORT, "Auto Export on Node Update", false);
  node.add_attr<BoolAttribute>(A_ADD_PREFIX, "Add Project Name as Prefix", false);
  // clang-format on

  // --- Attribute(s) order

  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Export",
                             A_FILENAME,
                             A_FORMAT,
                             A_AUTO_EXPORT,
                             A_ADD_PREFIX,
                             "_GROUPBOX_END_"});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_export_heightmap_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs

  auto *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (!p_in)
    return;

  // --- Params

  // clang-format off
  const auto auto_export = node.get_attr<BoolAttribute>(A_AUTO_EXPORT);
  auto fname             = node.get_attr<FilenameAttribute>(A_FILENAME);
  const auto format      = node.get_attr<EnumAttribute>(A_FORMAT);
  const auto add_prefix  = node.get_attr<BoolAttribute>(A_ADD_PREFIX);
  // clang-format on

  if (!auto_export)
    return;

  // --- Prepare filename

  if (add_prefix)
    fname = prepend_project_name_to_path(fname);

  // --- Convert input

  hmap::Array array = p_in->to_array(node.cfg().cm_cpu);

  // --- Export

  switch (format)
  {
  case ExportFormat::PNG8BIT:
  {
    fname = ensure_extension(fname, ".png");
    array.to_png_grayscale(fname.string(), CV_8U);
    break;
  }

  case ExportFormat::PNG16BIT:
  {
    fname = ensure_extension(fname, ".png");
    array.to_png_grayscale(fname.string(), CV_16U);
    break;
  }

  case ExportFormat::EXR32BIT:
  {
    fname = ensure_extension(fname, ".exr");
    array.to_exr(fname.string());
    break;
  }

  case ExportFormat::RAW16BIT:
  {
    fname = ensure_extension(fname, ".raw");
    array.to_raw_16bit(fname.string());
    break;
  }
  }
}

} // namespace hesiod
