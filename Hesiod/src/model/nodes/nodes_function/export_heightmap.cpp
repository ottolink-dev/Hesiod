/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/export.hpp"

#include "hesiod/app/enum_mappings.hpp"
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

constexpr const char *P_IN = "input";

constexpr const char *A_FILENAME    = "fname";
constexpr const char *A_FORMAT      = "format";
constexpr const char *A_AUTO_EXPORT = "auto_export";
constexpr const char *A_ADD_PREFIX  = "add_prefix";
constexpr const char *A_FORCE_SHAPE = "force_shape";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_export_heightmap_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);

  // --- Attributes

  std::vector<std::string> choices = {"Unchanged", "2^N", "2^N + 1"};

  node.set_current_category("Filename");
  add_filename(node, A_FILENAME, "Filename", "hmap.png", "*", true);
  add_bool(node, A_ADD_PREFIX, "Add Project Name as Prefix", false);

  node.set_current_category("Export Parameters");
  add_enum(node,
           A_FORMAT,
           "File Format",
           enum_mappings.heightmap_export_format_map,
           "png (16 bit)");
  add_bool(node, A_AUTO_EXPORT, "Auto Export on Node Update", false);
  add_choice(node, A_FORCE_SHAPE, "Force Export Shape", choices, "Unchanged");
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

  const auto auto_export = node.val<bool>(A_AUTO_EXPORT);
  auto       fname       = node.val<std::filesystem::path>(A_FILENAME);
  const auto format      = node.val<int>(A_FORMAT);
  const auto add_prefix  = node.val<bool>(A_ADD_PREFIX);
  const auto force_shape = node.val<std::string>(A_FORCE_SHAPE);

  if (!auto_export)
    return;

  // --- Prepare filename

  if (add_prefix)
    fname = prepend_project_name_to_path(fname);

  // --- Convert input

  hmap::Array array = p_in->to_array(node.cfg().cm_cpu);

  // --- Force array shape if requested

  if (force_shape != "Unchanged")
  {
    int        px        = (int)std::log2(node.cfg().shape.x);
    int        py        = (int)std::log2(node.cfg().shape.y);
    glm::ivec2 new_shape = {std::pow(2, px), std::pow(2, py)};

    if (force_shape == "2^N + 1")
    {
      new_shape.x++;
      new_shape.y++;
    }

    Logger::log()->trace("compute_export_heightmap_node: export shape = ({}, {})",
                         new_shape.x,
                         new_shape.y);

    array = array.resample_to_shape_bicubic(new_shape);
  }

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
