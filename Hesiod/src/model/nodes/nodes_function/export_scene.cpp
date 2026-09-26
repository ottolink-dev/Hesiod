/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/export.hpp"
#include "highmap/geometry/cloud.hpp"
#include "highmap/geometry/path.hpp"
#include "highmap/texture.hpp"
#include "highmap/transform.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/node_factory.hpp"
#include "hesiod/model/nodes/post_process.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_HEIGHTMAP = "heightmap";
constexpr const char *P_TEXTURE   = "texture";
constexpr const char *P_CLOUD_1   = "cloud 1";
constexpr const char *P_CLOUD_2   = "cloud 2";
constexpr const char *P_PATH_1    = "path 1";
constexpr const char *P_PATH_2    = "path 2";

constexpr const char *A_FNAME             = "fname";
constexpr const char *A_PATTERN           = "pattern";
constexpr const char *A_AUTO_EXPORT       = "auto_export";
constexpr const char *A_MESH_TYPE         = "mesh_type";
constexpr const char *A_MAX_ERROR         = "max_error";
constexpr const char *A_ELEVATION_SCALING = "elevation_scaling";
constexpr const char *A_FIT_BOUNDARIES    = "fit_boundaries";
constexpr const char *A_FLIP_X            = "flip_x";
constexpr const char *A_FLIP_Y            = "flip_y";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_export_scene_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // ports
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_HEIGHTMAP);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_TEXTURE);
  node.add_port<hmap::Cloud>(gnode::PortType::IN, P_CLOUD_1);
  node.add_port<hmap::Cloud>(gnode::PortType::IN, P_CLOUD_2);
  node.add_port<hmap::Path>(gnode::PortType::IN, P_PATH_1);
  node.add_port<hmap::Path>(gnode::PortType::IN, P_PATH_2);

  // enums
  std::map<std::string, int> mesh_type_map;

  for (auto &[id, infos] : hmap::mesh_type_as_string)
    mesh_type_map[infos] = (int)id;

  // attributes
  // clang-format off
  node.set_current_category("Filename");
  add_filename(node, A_FNAME, "Export File", std::filesystem::path("scene.usdc"), "Universal Scene Description (*.usda *.usdc *.usdz);;USDA (*.usda);;USDC (*.usdc);;USDZ (*.usdz)", true);
  add_string(node, A_PATTERN, "Filename Pattern", "{FILENAME}.{EXT}");
  add_bool(node, A_AUTO_EXPORT, "Auto Export on Node Update", false);

  node.set_current_category("Scene Parameters");
  add_enum(node, A_MESH_TYPE, "Mesh Type:", mesh_type_map, "triangles");
  add_float(node, A_MAX_ERROR, "Max Error", 5e-4f, 0.f, 0.01f);
  add_float(node, A_ELEVATION_SCALING, "Elevation Scale", 0.2f, 0.f, 1.f);
  add_bool(node, A_FIT_BOUNDARIES, "Fit Boundaries", false);
  add_bool(node, A_FLIP_X, "Flip-X", false);
  add_bool(node, A_FLIP_Y, "Flip-Y", false);
  // clang-format on
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_export_scene_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs

  auto *p_elev   = node.get_value_ref<hmap::VirtualArray>(P_HEIGHTMAP);
  auto *p_color  = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE);
  auto *p_cloud1 = node.get_value_ref<hmap::Cloud>(P_CLOUD_1);
  auto *p_cloud2 = node.get_value_ref<hmap::Cloud>(P_CLOUD_2);
  auto *p_path1  = node.get_value_ref<hmap::Path>(P_PATH_1);
  auto *p_path2  = node.get_value_ref<hmap::Path>(P_PATH_2);

  if (!p_elev)
    return;

  const bool auto_export = node.val<bool>(A_AUTO_EXPORT);
  if (!auto_export)
  {
    Logger::log()->trace("compute_export_scene_node: [{}]/[{}]: auto export is disabled",
                         node.get_node_type(),
                         node.get_id());
    return;
  }

  // --- Params

  // clang-format off
  auto       fpath          = node.val<std::filesystem::path>(A_FNAME);
  const auto pattern        = node.val<std::string>(A_PATTERN);
  const auto mesh_type      = node.val_enum<hmap::MeshType>(A_MESH_TYPE);
  const auto max_error      = node.val<float>(A_MAX_ERROR);
  const auto elev_scale     = node.val<float>(A_ELEVATION_SCALING);
  const auto fit_boundaries = node.val<bool>(A_FIT_BOUNDARIES);
  const auto flip_x         = node.val<bool>(A_FLIP_X);
  const auto flip_y         = node.val<bool>(A_FLIP_Y);
  // clang-format on

  // --- Resolve path using make_unique_filename

  std::unordered_map<std::string, std::string> replacements = get_standard_replacements(
      node,
      fpath);

  std::filesystem::path export_path = make_unique_filename(fpath.parent_path(),
                                                           pattern,
                                                           replacements);

  Logger::log()->trace("compute_export_scene_node: [{}]/[{}]: export path = {}",
                       node.get_node_type(),
                       node.get_id(),
                       export_path.string());

  const std::string fname = export_path.string();

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

  // --- Gather clouds (optional)

  std::vector<hmap::Cloud> clouds;
  auto                     process_cloud = [&](hmap::Cloud *p_c)
  {
    if (p_c && p_c->size() > 0)
    {
      if (flip_x || flip_y)
      {
        auto x_pts = p_c->get_x();
        auto y_pts = p_c->get_y();
        for (size_t i = 0; i < p_c->size(); ++i)
        {
          if (flip_x)
            x_pts[i] = 1.f - x_pts[i];
          if (flip_y)
            y_pts[i] = 1.f - y_pts[i];
        }
        hmap::Cloud c(x_pts, y_pts);
        c.set_values(p_c->get_values());
        clouds.push_back(c);
      }
      else
      {
        clouds.push_back(*p_c);
      }
    }
  };
  process_cloud(p_cloud1);
  process_cloud(p_cloud2);

  // --- Gather paths (optional)

  std::vector<hmap::Path> paths;
  auto                    process_path = [&](hmap::Path *p_p)
  {
    if (p_p && p_p->size() > 0)
    {
      if (flip_x || flip_y)
      {
        auto x_pts = p_p->get_x();
        auto y_pts = p_p->get_y();
        for (size_t i = 0; i < p_p->size(); ++i)
        {
          if (flip_x)
            x_pts[i] = 1.f - x_pts[i];
          if (flip_y)
            y_pts[i] = 1.f - y_pts[i];
        }
        hmap::Path p(x_pts, y_pts);
        p.set_values(p_p->get_values());
        paths.push_back(p);
      }
      else
      {
        paths.push_back(*p_p);
      }
    }
  };
  process_path(p_path1);
  process_path(p_path2);

  // --- Export scene

  hmap::export_usd(fname,
                   array,
                   clouds,
                   paths,
                   mesh_type,
                   elev_scale,
                   texture_fname,
                   "",
                   max_error,
                   fit_boundaries);
}

} // namespace hesiod
