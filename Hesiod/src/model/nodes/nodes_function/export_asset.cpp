/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/colorize.hpp"
#include "highmap/export.hpp"
#include "highmap/gradient.hpp"
#include "highmap/tensor.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/node_factory.hpp"
#include "hesiod/model/nodes/post_process.hpp"
#include "hesiod/model/utils.hpp"

using namespace attr;

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_ELEVATION = "elevation";
constexpr const char *P_TEXTURE = "texture";
constexpr const char *P_NORMAL_MAP = "normal map details";
constexpr const char *P_MASK = "mask";

constexpr const char *A_FNAME = "fname";
constexpr const char *A_AUTO_EXPORT = "auto_export";
constexpr const char *A_ADD_PREFIX = "add_prefix";
constexpr const char *A_EXPORT_FORMAT = "export_format";
constexpr const char *A_MESH_TYPE = "mesh_type";
constexpr const char *A_MAX_ERROR = "max_error";
constexpr const char *A_ELEVATION_SCALING = "elevation_scaling";
constexpr const char *A_DETAIL_SCALING = "detail_scaling";
constexpr const char *A_BLENDING_METHOD = "blending_method";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_export_asset_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // ports
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ELEVATION);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_TEXTURE);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_NORMAL_MAP);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);

  // attributes
  node.add_attr<FilenameAttribute>(A_FNAME,
                                   "Export File",
                                   std::filesystem::path("export"),
                                   "*",
                                   true);

  // attribute(s)
  // clang-format off
  node.add_attr<BoolAttribute>(A_AUTO_EXPORT, "Auto Export on Node Update", false);
  node.add_attr<BoolAttribute>(A_ADD_PREFIX, "Add Project Name as Prefix", false);
  node.add_attr<FloatAttribute>(A_MAX_ERROR, "Max Error", 5e-4f, 0.f, 0.01f);
  node.add_attr<FloatAttribute>(A_ELEVATION_SCALING, "Elevation Scale", 0.2f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_DETAIL_SCALING, "Normal Map Scale", 1.f, 0.f, 4.f);
  node.add_attr<EnumAttribute>(A_BLENDING_METHOD, "Blending Method:", hmap::normal_map_blending_method_as_string);
  // clang-format on

  // enums
  {
    std::map<std::string, int> export_format_map;
    std::map<std::string, int> mesh_type_map;

    for (auto &[id, infos] : hmap::asset_export_format_as_string)
      export_format_map[infos[0]] = (int)id;

    for (auto &[id, infos] : hmap::mesh_type_as_string)
      mesh_type_map[infos] = (int)id;

    // clang-format off
    node.add_attr<EnumAttribute>(A_EXPORT_FORMAT, "Export Format:", export_format_map, "GL Transmission Format v. 2 (binary) - *.glb");
    node.add_attr<EnumAttribute>(A_MESH_TYPE, "Mesh Type:", mesh_type_map, "triangles");
    // clang-format on
  }

  // attribute(s) order
  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Export",
                             A_AUTO_EXPORT,
                             A_FNAME,
                             A_ADD_PREFIX,
                             A_EXPORT_FORMAT,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Geometry",
                             A_MESH_TYPE,
                             A_MAX_ERROR,
                             A_ELEVATION_SCALING,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Shading",
                             A_BLENDING_METHOD,
                             A_DETAIL_SCALING,
                             "_GROUPBOX_END_"});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_export_asset_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs

  auto *p_elev = node.get_value_ref<hmap::VirtualArray>(P_ELEVATION);
  auto *p_color = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE);
  auto *p_nmap = node.get_value_ref<hmap::VirtualTexture>(P_NORMAL_MAP);
  auto *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);

  if (!p_elev)
    return;

  const bool auto_export = node.get_attr<BoolAttribute>(A_AUTO_EXPORT);
  if (!auto_export)
    return;

  // --- Params

  // clang-format off
  auto       fpath           = node.get_attr<FilenameAttribute>(A_FNAME);
  const auto add_prefix      = node.get_attr<BoolAttribute>(A_ADD_PREFIX);
  const auto export_format   = node.get_attr<EnumAttribute>(A_EXPORT_FORMAT);
  const auto mesh_type       = node.get_attr<EnumAttribute>(A_MESH_TYPE);
  const auto max_error       = node.get_attr<FloatAttribute>(A_MAX_ERROR);
  const auto elev_scale      = node.get_attr<FloatAttribute>(A_ELEVATION_SCALING);
  const auto detail_scale    = node.get_attr<FloatAttribute>(A_DETAIL_SCALING);
  const auto blending_method = node.get_attr<EnumAttribute>(A_BLENDING_METHOD);
  // clang-format on

  // --- Resolve path

  if (add_prefix)
    fpath = prepend_project_name_to_path(fpath);

  const std::string fname = fpath.string();

  // --- Convert elevation

  hmap::Array array = p_elev->to_array(node.cfg().cm_cpu);

  // --- Export texture (optional)

  std::string texture_fname;

  if (p_color)
  {
    texture_fname = fname + ".png";
    p_color->to_png(texture_fname, node.cfg().cm_cpu, CV_16U);
  }

  // --- Build normal map

  std::string nmap_fname = fname + "_nmap.png";

  hmap::Tensor nvec = hmap::normal_map(array);

  hmap::Array nx = nvec.get_slice(0);
  hmap::Array ny = nvec.get_slice(1);
  hmap::Array nz = nvec.get_slice(2);
  hmap::Array alpha(node.cfg().shape, 1.f);

  hmap::VirtualTexture normal_map(CONFIG_TEX(node));
  normal_map.from_arrays({&nx, &ny, &nz, &alpha}, node.cfg().cm_cpu);

  // --- Blend detail normal map

  if (p_nmap)
  {
    hmap::mix_normal_map(normal_map,
                         normal_map,
                         *p_nmap,
                         node.cfg().cm_cpu,
                         detail_scale,
                         (hmap::NormalMapBlendingMethod)blending_method);
  }

  normal_map.to_png(nmap_fname, node.cfg().cm_cpu, CV_16U);

  // --- Export asset

  if (p_mask)
  {
    hmap::Array mask = p_mask->to_array(node.cfg().cm_cpu);

    // with a mask, the optimized mesh is not available
    hmap::export_asset(fname,
                       array,
                       mask,
                       (hmap::AssetExportFormat)export_format,
                       elev_scale,
                       texture_fname,
                       nmap_fname);
  }
  else
  {
    hmap::export_asset(fname,
                       array,
                       (hmap::MeshType)mesh_type,
                       (hmap::AssetExportFormat)export_format,
                       elev_scale,
                       texture_fname,
                       nmap_fname,
                       max_error);
  }
}

} // namespace hesiod
