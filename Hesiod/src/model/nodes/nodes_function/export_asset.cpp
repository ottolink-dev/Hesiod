/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/colorize.hpp"
#include "highmap/export.hpp"
#include "highmap/gradient.hpp"
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

constexpr const char *P_ELEVATION  = "elevation";
constexpr const char *P_TEXTURE    = "texture";
constexpr const char *P_NORMAL_MAP = "normal map details";
constexpr const char *P_MASK       = "mask";

constexpr const char *A_FNAME             = "fname";
constexpr const char *A_AUTO_EXPORT       = "auto_export";
constexpr const char *A_ADD_PREFIX        = "add_prefix";
constexpr const char *A_EXPORT_FORMAT     = "export_format";
constexpr const char *A_MESH_TYPE         = "mesh_type";
constexpr const char *A_MAX_ERROR         = "max_error";
constexpr const char *A_ELEVATION_SCALING = "elevation_scaling";
constexpr const char *A_DETAIL_SCALING    = "detail_scaling";
constexpr const char *A_BLENDING_METHOD   = "blending_method";
constexpr const char *A_FLIP_X            = "flip_x";
constexpr const char *A_FLIP_Y            = "flip_y";

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
  add_filename(node, A_FNAME, "Export File", std::filesystem::path("export"), "*", true);

  // attribute(s)
  // clang-format off
  add_bool(node, A_AUTO_EXPORT, "Auto Export on Node Update", false);
  add_bool(node, A_ADD_PREFIX, "Add Project Name as Prefix", false);
  add_float(node, A_MAX_ERROR, "Max Error", 5e-4f, 0.f, 0.01f);
  add_float(node, A_ELEVATION_SCALING, "Elevation Scale", 0.2f, 0.f, 1.f);
  add_float(node, A_DETAIL_SCALING, "Normal Map Scale", 1.f, 0.f, 4.f);
  add_enum(node, A_BLENDING_METHOD, "Blending Method:", hmap::normal_map_blending_method_as_string);
  add_bool(node, A_FLIP_X, "Flip-X", false);
  add_bool(node, A_FLIP_Y, "Flip-Y", false);
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
    add_enum(node, A_EXPORT_FORMAT, "Export Format:", export_format_map, "GL Transmission Format v. 2 (binary) - *.glb");
    add_enum(node, A_MESH_TYPE, "Mesh Type:", mesh_type_map, "triangles");
    // clang-format on
  }
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_export_asset_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs

  auto *p_elev  = node.get_value_ref<hmap::VirtualArray>(P_ELEVATION);
  auto *p_color = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE);
  auto *p_nmap  = node.get_value_ref<hmap::VirtualTexture>(P_NORMAL_MAP);
  auto *p_mask  = node.get_value_ref<hmap::VirtualArray>(P_MASK);

  if (!p_elev)
    return;

  const bool auto_export = node.val<bool>(A_AUTO_EXPORT);
  if (!auto_export)
    return;

  // --- Params

  // clang-format off
  auto       fpath           = node.val<std::filesystem::path>(A_FNAME);
  const auto add_prefix      = node.val<bool>(A_ADD_PREFIX);
  const auto export_format   = node.val<int>(A_EXPORT_FORMAT);
  const auto mesh_type       = node.val<int>(A_MESH_TYPE);
  const auto max_error       = node.val<float>(A_MAX_ERROR);
  const auto elev_scale      = node.val<float>(A_ELEVATION_SCALING);
  const auto detail_scale    = node.val<float>(A_DETAIL_SCALING);
  const auto blending_method = node.val<int>(A_BLENDING_METHOD);
  const auto flip_x          = node.val<bool>(A_FLIP_X);
  const auto flip_y          = node.val<bool>(A_FLIP_Y);
  // clang-format on

  // --- Resolve path

  if (add_prefix)
    fpath = prepend_project_name_to_path(fpath);

  const std::string fname = fpath.string();

  // --- Convert elevation

  hmap::Array array = p_elev->to_array(node.cfg().cm_cpu);
  if (flip_x)
    hmap::flip_lr(array);
  if (flip_y)
    hmap::flip_ud(array);

  // --- Export texture (optional)

  std::string texture_fname;

  if (p_color)
  {
    texture_fname   = fname + ".png";
    hmap::Texture t = p_color->to_texture(p_color->shape, node.cfg().cm_cpu);
    if (flip_x)
      hmap::flip_lr(t);
    if (flip_y)
      hmap::flip_ud(t);
    t.to_png(texture_fname, CV_16U);
  }

  // --- Build normal map

  std::string nmap_fname = fname + "_nmap.png";

  hmap::Texture nvec = hmap::normal_map(array);

  hmap::Array nx = nvec.channel(0);
  hmap::Array ny = nvec.channel(1);
  hmap::Array nz = nvec.channel(2);
  hmap::Array alpha(node.cfg().shape, 1.f);

  hmap::VirtualTexture normal_map(CONFIG_TEX(node));
  normal_map.from_arrays({&nx, &ny, &nz, &alpha}, node.cfg().cm_cpu);

  // --- Blend detail normal map

  if (p_nmap)
  {
    hmap::Texture t_nmap = p_nmap->to_texture(p_nmap->shape, node.cfg().cm_cpu);
    if (flip_x)
      hmap::flip_lr(t_nmap);
    if (flip_y)
      hmap::flip_ud(t_nmap);
    hmap::VirtualTexture             flipped_nmap(CONFIG_TEX(node));
    std::vector<const hmap::Array *> nmap_ptrs;
    for (auto &c : t_nmap.channels)
      nmap_ptrs.push_back(&c);
    flipped_nmap.from_arrays(nmap_ptrs, node.cfg().cm_cpu);

    hmap::mix_normal_map(normal_map,
                         normal_map,
                         flipped_nmap,
                         node.cfg().cm_cpu,
                         detail_scale,
                         (hmap::NormalMapBlendingMethod)blending_method);
  }

  normal_map.to_png(nmap_fname, node.cfg().cm_cpu, CV_16U);

  // --- Export asset

  if (p_mask)
  {
    hmap::Array mask = p_mask->to_array(node.cfg().cm_cpu);
    if (flip_x)
      hmap::flip_lr(mask);
    if (flip_y)
      hmap::flip_ud(mask);

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
