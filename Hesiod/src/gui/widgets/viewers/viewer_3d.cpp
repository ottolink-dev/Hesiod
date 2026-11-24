/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cmath>
#include <format>

#include <QColor>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "highmap/geometry/cloud.hpp"
#include "highmap/geometry/path.hpp"
#include "highmap/range.hpp"
#include "highmap/world/flora/forest.hpp"
#include "highmap/world/rocks/rock_field.hpp"

#include "oros/scene/scene_types.hpp"
#include "oros/terrain/heightmap_data.hpp"
#include "oros/ui/viewport_widget.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/controls/icon_check_box.hpp"
#include "hesiod/gui/widgets/graph/graph_node_widget.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"
#include "hesiod/gui/widgets/viewers/render_helpers.hpp"
#include "hesiod/gui/widgets/viewers/viewer_3d.hpp"
#include "hesiod/gui/widgets/viewers/viewport/viewport_style.hpp"
#include "hesiod/gui/widgets/viewers/viewport_controls.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/graph/graph_node.hpp"
#include "hesiod/profiling.hpp"

namespace hesiod
{

namespace
{

constexpr float kWorldLx = 1000.0f;
constexpr float kWorldLy = 1000.0f;
constexpr float kBaseHeightScale = 200.0f;

// Convert Hesiod RGBA8 image buffer to Oros RGB8
std::vector<uint8_t> rgba8_to_rgb8(const std::vector<uint8_t> &rgba)
{
  const size_t         pixels = rgba.size() / 4;
  std::vector<uint8_t> rgb(pixels * 3);
  for (size_t i = 0; i < pixels; ++i)
  {
    rgb[i * 3 + 0] = rgba[i * 4 + 0];
    rgb[i * 3 + 1] = rgba[i * 4 + 1];
    rgb[i * 3 + 2] = rgba[i * 4 + 2];
  }
  return rgb;
}

oros::terrain::Vec3 norm_to_world_xy(float nx, float ny, float z)
{
  return {(nx - 0.5f) * kWorldLx, (ny - 0.5f) * kWorldLy, z};
}

oros::scene::MeshGeometryData create_point_sphere_mesh(float radius = 3.0f)
{
  oros::scene::MeshGeometryData mesh;
  constexpr int                 stacks = 6;
  constexpr int                 slices = 10;

  for (int i = 0; i <= stacks; ++i)
  {
    const float phi = float(M_PI) * float(i) / float(stacks);
    const float sp = std::sin(phi);
    const float cp = std::cos(phi);

    for (int j = 0; j <= slices; ++j)
    {
      const float theta = 2.0f * float(M_PI) * float(j) / float(slices);
      const float x = sp * std::cos(theta);
      const float y = sp * std::sin(theta);
      const float z = cp;

      mesh.positions.push_back({x * radius, y * radius, z * radius});
      mesh.normals.push_back({x, y, z});
      mesh.colors.push_back({1.0f, 1.0f, 1.0f});
    }
  }

  for (int i = 0; i < stacks; ++i)
  {
    for (int j = 0; j < slices; ++j)
    {
      const uint32_t a = static_cast<uint32_t>(i * (slices + 1) + j);
      const uint32_t b = a + static_cast<uint32_t>(slices + 1);
      mesh.indices.push_back(a);
      mesh.indices.push_back(b);
      mesh.indices.push_back(a + 1);
      mesh.indices.push_back(a + 1);
      mesh.indices.push_back(b);
      mesh.indices.push_back(b + 1);
    }
  }

  return mesh;
}

} // namespace

// 1 port
template <typename T, typename F>
bool helper_try_set_from_port(const BaseNode       &node,
                              const std::string    &port_name,
                              const std::type_info &type,
                              F                   &&fn)
{
  if (port_name.empty())
    return false;

  int pid = node.get_port_index(port_name);

  if (pid < 0)
    return false;
  if (node.get_data_type(pid) != type.name())
    return false;

  T *ptr = node.get_value_ref<T>(pid);

  if (!ptr)
    return false;

  fn(*ptr);
  return true;
}

// 2 ports
template <typename T, typename F>
bool helper_try_set_from_port(const BaseNode       &node,
                              const std::string    &port_name1,
                              const std::string    &port_name2,
                              const std::type_info &type,
                              F                   &&fn)
{
  if (port_name1.empty() || port_name2.empty())
    return false;

  int pid1 = node.get_port_index(port_name1);
  if (pid1 < 0)
    return false;
  if (node.get_data_type(pid1) != type.name())
    return false;

  int pid2 = node.get_port_index(port_name2);
  if (pid2 < 0)
    return false;
  if (node.get_data_type(pid2) != type.name())
    return false;

  T *ptr1 = node.get_value_ref<T>(pid1);
  T *ptr2 = node.get_value_ref<T>(pid2);

  if (!ptr1 || !ptr2)
    return false;

  fn(*ptr1, *ptr2);
  return true;
}

// =====================================
// Viewer3D - class definition
// =====================================

Viewer3D::Viewer3D(QPointer<GraphNodeWidget> p_graph_node_widget_, QWidget *parent)
    : Viewer(p_graph_node_widget_, ViewerType::VIEWER3D, "3D Renderer", parent)
{
  Logger::log()->trace("Viewer3D::Viewer3D");

  this->view_param = this->get_default_view_param();
  for (const auto &[k, _] : this->view_param.port_ids)
    this->param_visibility_state[k] = true;

  this->setup_layout();
  this->update_widgets();
  this->setup_connections();
  this->update_param_visibility_icons();
}

void Viewer3D::copy_render_settings_from(const Viewer3D &source)
{
  if (!this->p_renderer || !source.p_renderer)
    return;

  this->p_renderer->set_sun_settings(source.p_renderer->get_sun_settings());
  this->p_renderer->set_environment_settings(
      source.p_renderer->get_environment_settings());
  this->p_renderer->set_water_settings(source.p_renderer->get_water_settings());
  this->p_renderer->set_fov_deg(source.p_renderer->get_fov_deg());
  this->p_renderer->set_height_scale_multiplier(
      source.p_renderer->get_height_scale_multiplier());
  this->p_renderer->set_background_color(source.p_renderer->get_background_color());
  this->update_param_visibility_icons();
}

void Viewer3D::clear()
{
  Viewer::clear();
  if (this->p_renderer)
  {
    this->p_renderer->clear_terrains();
    this->p_renderer->clear_waters();
    this->p_renderer->clear_instanced_groups();
    this->p_renderer->clear_paths();
  }
}

ViewerNodeParam Viewer3D::get_default_view_param() const
{
  ViewerNodeParam wp;

  wp.port_ids = {
      {"elevation", ""},
      {"water_depth", ""},
      {"color", ""},
      {"normal_map", ""},
      {"points", ""},
      {"path", ""},
      {"trees", "forest"},
      {"rocks", "rock_field"},
  };

  wp.icons = {
      {"elevation", HSD_ICON("landscape")},
      {"water_depth", HSD_ICON("waves")},
      {"color", HSD_ICON("palette")},
      {"normal_map", HSD_ICON("hdr_strong")},
      {"points", HSD_ICON("scatter_plot")},
      {"path", HSD_ICON("conversion_path")},
      {"trees", HSD_ICON("forest")},
      {"rocks", HSD_ICON("landslide")},
  };

  return wp;
}

bool Viewer3D::get_param_visibility_state(const std::string &param_name) const
{
  auto it = this->param_visibility_state.find(param_name);
  return it != this->param_visibility_state.end() ? it->second : true;
}

void Viewer3D::json_from(nlohmann::json const &json)
{
  Logger::log()->trace("Viewer3D::json_from");

  Viewer::json_from(json);

  if (this->p_renderer && json.contains("oros_renderer"))
  {
    const auto &r = json["oros_renderer"];
    if (r.contains("sun"))
    {
      auto s = this->p_renderer->get_sun_settings();
      s.azimuth_deg = r["sun"].value("azimuth_deg", s.azimuth_deg);
      s.elevation_deg = r["sun"].value("elevation_deg", s.elevation_deg);
      s.intensity = r["sun"].value("intensity", s.intensity);
      s.auto_color_temperature = r["sun"].value("auto_color_temperature",
                                                s.auto_color_temperature);
      s.render_sun_disk = r["sun"].value("render_sun_disk", s.render_sun_disk);
      this->p_renderer->set_sun_settings(s);
    }
    if (r.contains("env"))
    {
      auto e = this->p_renderer->get_environment_settings();
      e.pbr_enabled = r["env"].value("pbr_enabled", e.pbr_enabled);
      e.soft_shadows_enabled = r["env"].value("soft_shadows_enabled",
                                              e.soft_shadows_enabled);
      e.ambient_occlusion_enabled = r["env"].value("ambient_occlusion_enabled",
                                                   e.ambient_occlusion_enabled);
      e.fog_enabled = r["env"].value("fog_enabled", e.fog_enabled);
      e.fog_density = r["env"].value("fog_density", e.fog_density);
      e.fog_start = r["env"].value("fog_start", e.fog_start);
      e.fog_height = r["env"].value("fog_height", e.fog_height);
      e.fog_height_falloff = r["env"].value("fog_height_falloff", e.fog_height_falloff);
      e.atmospheric_scattering_enabled = r["env"].value("atmospheric_scattering_enabled",
                                                        e.atmospheric_scattering_enabled);
      e.atmosphere_turbidity = r["env"].value("atmosphere_turbidity",
                                              e.atmosphere_turbidity);
      e.rayleigh_strength = r["env"].value("rayleigh_strength", e.rayleigh_strength);
      e.mie_strength = r["env"].value("mie_strength", e.mie_strength);
      e.ground_plane_enabled = r["env"].value("ground_plane_enabled",
                                              e.ground_plane_enabled);
      e.ground_plane_elevation = r["env"].value("ground_plane_elevation",
                                                e.ground_plane_elevation);
      if (r["env"].contains("ground_plane_color") &&
          r["env"]["ground_plane_color"].is_array() &&
          r["env"]["ground_plane_color"].size() == 3)
      {
        e.ground_plane_color = {r["env"]["ground_plane_color"][0].get<float>(),
                                r["env"]["ground_plane_color"][1].get<float>(),
                                r["env"]["ground_plane_color"][2].get<float>()};
      }
      this->p_renderer->set_environment_settings(e);
    }
    if (r.contains("fov_deg"))
      this->p_renderer->set_fov_deg(r["fov_deg"].get<float>());
    if (r.contains("height_scale_multiplier"))
      this->p_renderer->set_height_scale_multiplier(
          r["height_scale_multiplier"].get<float>());
  }

  if (this->controls && json.contains("viewport_controls"))
    this->controls->json_from(json["viewport_controls"]);

  this->update_param_visibility_icons();
}

nlohmann::json Viewer3D::json_to() const
{
  Logger::log()->trace("Viewer3D::json_to");

  nlohmann::json json = Viewer::json_to();
  if (this->p_renderer)
  {
    const auto    &s = this->p_renderer->get_sun_settings();
    const auto    &e = this->p_renderer->get_environment_settings();
    nlohmann::json r;
    r["sun"] = {{"azimuth_deg", s.azimuth_deg},
                {"elevation_deg", s.elevation_deg},
                {"intensity", s.intensity},
                {"auto_color_temperature", s.auto_color_temperature},
                {"render_sun_disk", s.render_sun_disk}};
    r["env"] = {
        {"pbr_enabled", e.pbr_enabled},
        {"soft_shadows_enabled", e.soft_shadows_enabled},
        {"ambient_occlusion_enabled", e.ambient_occlusion_enabled},
        {"fog_enabled", e.fog_enabled},
        {"fog_density", e.fog_density},
        {"fog_start", e.fog_start},
        {"fog_height", e.fog_height},
        {"fog_height_falloff", e.fog_height_falloff},
        {"atmospheric_scattering_enabled", e.atmospheric_scattering_enabled},
        {"atmosphere_turbidity", e.atmosphere_turbidity},
        {"rayleigh_strength", e.rayleigh_strength},
        {"mie_strength", e.mie_strength},
        {"ground_plane_enabled", e.ground_plane_enabled},
        {"ground_plane_elevation", e.ground_plane_elevation},
        {"ground_plane_color",
         {e.ground_plane_color.x, e.ground_plane_color.y, e.ground_plane_color.z}}};
    r["fov_deg"] = this->p_renderer->get_fov_deg();
    r["height_scale_multiplier"] = this->p_renderer->get_height_scale_multiplier();
    json["oros_renderer"] = r;
  }

  if (this->controls)
    json["viewport_controls"] = this->controls->json_to();

  return json;
}

void Viewer3D::on_view_param_visibility_changed(const std::string &param_name,
                                                bool               new_state)
{
  this->param_visibility_state[param_name] = new_state;
  if (!this->p_renderer)
    return;

  if (param_name == "elevation")
    this->p_renderer->set_terrain_visible("main_terrain", new_state);
  else if (param_name == "water_depth")
    this->p_renderer->set_water_visible("main_water", new_state);
  else if (param_name == "points")
    this->p_renderer->set_instanced_group_visible("main_points", new_state);
  else if (param_name == "path")
    this->p_renderer->set_path_visible("main_path", new_state);
  else if (param_name == "trees")
  {
    this->p_renderer->set_instanced_group_visible("main_trees_pine", new_state);
    this->p_renderer->set_instanced_group_visible("main_trees_deciduous", new_state);
  }
  else if (param_name == "rocks")
    this->p_renderer->set_instanced_group_visible("main_rocks", new_state);
  else if (param_name == "color")
    this->p_renderer->set_albedo_enabled(new_state);
  else if (param_name == "normal_map")
    this->p_renderer->set_normal_map_enabled(new_state);
}

void Viewer3D::resizeEvent(QResizeEvent *) {}

void Viewer3D::sync_pin_label()
{
  const bool has_node = !this->current_node_id.empty();
  if (!has_node)
    this->button_pin_current_node->set_label("No node previewed");
  this->button_pin_current_node->setEnabled(has_node);
}

void Viewer3D::setup_connections()
{
  Logger::log()->trace("Viewer3D::setup_connections");

  Viewer::setup_connections();

  this->connect(this,
                &Viewer::view_param_visibility_changed,
                this,
                &Viewer3D::on_view_param_visibility_changed);
}

void Viewer3D::setup_layout()
{
  Logger::log()->trace("Viewer3D::setup_layout");

  Viewer::setup_layout();

  QGridLayout *grid = dynamic_cast<QGridLayout *>(this->layout());
  int          row_count = get_row_count(grid);

  auto *container = new QWidget(this);
  auto *hbox = new QHBoxLayout(container);
  hbox->setContentsMargins(0, 0, 0, 0);
  hbox->setSpacing(0);

  this->p_renderer = new oros::ui::ViewportWidget(container);
  this->p_renderer->set_rendering_domain(kWorldLx, kWorldLy, kBaseHeightScale * 2.0f);
  {
    auto env = this->p_renderer->get_environment_settings();
    env.ground_plane_enabled = true;
    this->p_renderer->set_environment_settings(env);
  }
  this->update_style();
  hbox->addWidget(this->p_renderer, 1);

  // Dedicated rail host beside the native Filament viewport so the rail never
  // overlaps the WA_PaintOnScreen surface
  this->p_rail_host = new QWidget(container);
  this->p_rail_host->setFixedWidth(viewport::thick_px() + viewport::kMargin * 2);
  hbox->addWidget(this->p_rail_host, 0);

  grid->addWidget(container, 0, 0, row_count, 1);

  this->controls = new ViewportControls(this->p_renderer,
                                        this->p_rail_host,
                                        this->p_graph_node_widget,
                                        this);
  this->controls->on_layout_changed = [this]() { this->resizeEvent(nullptr); };

  {
    auto *content = new QWidget();
    auto *column = new QVBoxLayout(content);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(6);

    auto *pin_row = new QHBoxLayout();
    pin_row->setContentsMargins(8, 2, 8, 2);
    this->button_pin_current_node->setParent(content);
    this->button_pin_current_node->setToolTip(
        "Pin: keep the view on this node while other nodes are selected");
    pin_row->addWidget(this->button_pin_current_node);
    pin_row->addStretch(1);
    column->addLayout(pin_row);

    column->addWidget(this->combo_container);

    this->controls->set_preview_content(content);
    this->controls->on_preview_opened = [this]()
    {
      this->update_param_visibility_icons();
      this->sync_pin_label();
    };
    this->connect(this,
                  &Viewer::current_node_id_changed,
                  this,
                  [this](const std::string &) { this->sync_pin_label(); });
    this->sync_pin_label();
  }
}

void Viewer3D::restyle_all()
{
  for (QWidget *top : QApplication::topLevelWidgets())
    for (Viewer3D *viewer : top->findChildren<Viewer3D *>())
      viewer->update_style();
}

void Viewer3D::update_style()
{
  if (!this->p_renderer)
    return;

  const auto &colors = HSD_CTX.app_settings.colors;
  this->p_renderer->set_background_color(colors.bg_deep);
}

void Viewer3D::update_renderer()
{
  HSD_PROFILE_ZONE("Viewer3D::update_renderer");
  Logger::log()->trace("Viewer3D::update_renderer");

  if (!this->isVisible())
    return;

  if (!this->p_renderer)
  {
    Logger::log()->error(
        "Viewer3D::update_renderer: renderer reference is a dangling ptr");
    return;
  }

  if (this->current_node_id.empty())
  {
    this->p_renderer->clear_terrains();
    this->p_renderer->clear_waters();
    this->p_renderer->clear_instanced_groups();
    this->p_renderer->clear_paths();
    return;
  }

  BaseNode *p_node = this->safe_get_node();
  if (!p_node)
  {
    this->p_renderer->clear_terrains();
    this->p_renderer->clear_waters();
    this->p_renderer->clear_instanced_groups();
    this->p_renderer->clear_paths();
    return;
  }

  this->update_param_visibility_icons();

  bool                                          flip_y = false;
  std::shared_ptr<oros::terrain::HeightmapData> shared_hmap;

  // 1. Elevation + Albedo + Normal -> TerrainLayerDescriptor
  if (helper_try_set_from_port<hmap::VirtualArray>(
          *p_node,
          this->view_param.port_ids.at("elevation"),
          typeid(hmap::VirtualArray),
          [&](const hmap::VirtualArray &h)
          {
            HSD_PROFILE_ZONE("Viewer3D::elevation");
            auto               arr = h.to_array(p_node->cfg().cm_cpu);
            const size_t       nx = static_cast<size_t>(h.shape.x);
            const size_t       ny = static_cast<size_t>(h.shape.y);
            std::vector<float> row_major(nx * ny);
            for (size_t y = 0; y < ny; ++y)
              for (size_t x = 0; x < nx; ++x)
                row_major[y * nx + x] = arr(static_cast<int>(x), static_cast<int>(y));
            shared_hmap = std::make_shared<oros::terrain::HeightmapData>(
                nx,
                ny,
                std::move(row_major));

            oros::scene::TerrainLayerDescriptor desc;
            desc.id = "main_terrain";
            desc.heightmap = shared_hmap;
            desc.center = {0.0f, 0.0f, 0.0f};
            desc.lx = kWorldLx;
            desc.ly = kWorldLy;
            desc.height_scale = kBaseHeightScale;

            // Albedo
            if (!helper_try_set_from_port<hmap::VirtualArray>(
                    *p_node,
                    this->view_param.port_ids.at("color"),
                    typeid(hmap::VirtualArray),
                    [&](const hmap::VirtualArray &c)
                    {
                      auto carr = c.to_array(p_node->cfg().cm_cpu);
                      auto img = generate_selector_image(carr);
                      desc.albedo_map_rgb8 = rgba8_to_rgb8(img);
                      desc.texture_width = static_cast<size_t>(c.shape.x);
                      desc.texture_height = static_cast<size_t>(c.shape.y);
                    }))
            {
              helper_try_set_from_port<hmap::VirtualTexture>(
                  *p_node,
                  this->view_param.port_ids.at("color"),
                  typeid(hmap::VirtualTexture),
                  [&](const hmap::VirtualTexture &rgba)
                  {
                    auto img = rgba.to_img_8bit(rgba.shape, p_node->cfg().cm_cpu, flip_y);
                    desc.albedo_map_rgb8 = rgba8_to_rgb8(img);
                    desc.texture_width = static_cast<size_t>(rgba.shape.x);
                    desc.texture_height = static_cast<size_t>(rgba.shape.y);
                  });
            }

            if (desc.albedo_map_rgb8.empty())
            {
              desc.albedo_map_rgb8 = {160, 160, 160};
              desc.texture_width = 1;
              desc.texture_height = 1;
            }

            // Normal map
            helper_try_set_from_port<hmap::VirtualTexture>(
                *p_node,
                this->view_param.port_ids.at("normal_map"),
                typeid(hmap::VirtualTexture),
                [&](const hmap::VirtualTexture &rgba)
                {
                  auto img = rgba.to_img_8bit(rgba.shape, p_node->cfg().cm_cpu, flip_y);
                  desc.normal_map_rgb8 = rgba8_to_rgb8(img);
                  if (desc.texture_width == 0)
                  {
                    desc.texture_width = static_cast<size_t>(rgba.shape.x);
                    desc.texture_height = static_cast<size_t>(rgba.shape.y);
                  }
                });

            this->p_renderer->add_or_update_terrain(desc);
            this->p_renderer->set_terrain_visible(
                "main_terrain",
                this->get_param_visibility_state("elevation"));
          }))
  {
    // Terrain updated
  }
  else
  {
    this->p_renderer->remove_terrain("main_terrain");
  }

  // 2. Water -> WaterLayerDescriptor (simplified, no dilation or waves)
  if (shared_hmap &&
      helper_try_set_from_port<hmap::VirtualArray>(
          *p_node,
          this->view_param.port_ids.at("water_depth"),
          typeid(hmap::VirtualArray),
          [&](const hmap::VirtualArray &w)
          {
            HSD_PROFILE_ZONE("Viewer3D::water");
            auto               aw = w.to_array(p_node->cfg().cm_cpu);
            const size_t       nx = static_cast<size_t>(w.shape.x);
            const size_t       ny = static_cast<size_t>(w.shape.y);
            std::vector<float> row_major(nx * ny);
            for (size_t y = 0; y < ny; ++y)
              for (size_t x = 0; x < nx; ++x)
                row_major[y * nx + x] = aw(static_cast<int>(x), static_cast<int>(y));
            auto depth_map = std::make_shared<oros::terrain::HeightmapData>(
                nx,
                ny,
                std::move(row_major));

            oros::scene::WaterLayerDescriptor wdesc;
            wdesc.id = "main_water";
            wdesc.terrain_heightmap = shared_hmap;
            wdesc.water_depth_map = depth_map;
            wdesc.center = {0.0f, 0.0f, 0.0f};
            wdesc.lx = kWorldLx;
            wdesc.ly = kWorldLy;
            wdesc.height_scale = kBaseHeightScale;

            this->p_renderer->add_or_update_water(wdesc);
            this->p_renderer->set_water_visible(
                "main_water",
                this->get_param_visibility_state("water_depth"));
          }))
  {
    // Water updated
  }
  else
  {
    this->p_renderer->remove_water("main_water");
  }

  const float z_scale = kBaseHeightScale *
                        this->p_renderer->get_height_scale_multiplier();

  // 3. Points (hmap::Cloud)
  if (!helper_try_set_from_port<hmap::Cloud>(
          *p_node,
          this->view_param.port_ids.at("points"),
          typeid(hmap::Cloud),
          [&](const hmap::Cloud &c)
          {
            oros::scene::InstancedObjectGroup grp;
            grp.id = "main_points";
            grp.mesh_type = oros::scene::ScatterMeshType::CUSTOM;
            grp.custom_mesh = create_point_sphere_mesh(3.0f);
            grp.base_color = {0.2f, 0.75f, 1.0f};
            grp.roughness = 0.4f;

            const auto &xs = c.get_x();
            const auto &ys = c.get_y();
            const auto &vs = c.get_values();
            grp.instances.reserve(xs.size());
            for (size_t i = 0; i < xs.size(); ++i)
            {
              oros::scene::ObjectInstance inst;
              inst.position = norm_to_world_xy(xs[i], ys[i], vs[i] * z_scale);
              inst.scale = 1.0f;
              grp.instances.push_back(inst);
            }
            this->p_renderer->add_or_update_instanced_group(grp);
            this->p_renderer->set_instanced_group_visible(
                "main_points",
                this->get_param_visibility_state("points"));
          }))
  {
    this->p_renderer->remove_instanced_group("main_points");
  }

  // 4. Path (hmap::Path)
  if (!helper_try_set_from_port<hmap::Path>(
          *p_node,
          this->view_param.port_ids.at("path"),
          typeid(hmap::Path),
          [&](const hmap::Path &p)
          {
            oros::scene::PathDescriptor pdesc;
            pdesc.id = "main_path";
            const auto &xs = p.get_x();
            const auto &ys = p.get_y();
            const auto &vs = p.get_values();
            pdesc.points.reserve(xs.size());
            for (size_t i = 0; i < xs.size(); ++i)
              pdesc.points.push_back(norm_to_world_xy(xs[i], ys[i], vs[i] * z_scale));
            this->p_renderer->add_or_update_path(pdesc);
            this->p_renderer->set_path_visible("main_path",
                                               this->get_param_visibility_state("path"));
          }))
  {
    this->p_renderer->remove_path("main_path");
  }

  // 5. Trees (hmap::Forest)
  if (!helper_try_set_from_port<hmap::Forest>(
          *p_node,
          this->view_param.port_ids.at("trees"),
          typeid(hmap::Forest),
          [&](const hmap::Forest &f)
          {
            oros::scene::InstancedObjectGroup pines;
            pines.id = "main_trees_pine";
            pines.mesh_type = oros::scene::ScatterMeshType::PINE_TREE;
            pines.base_color = {1.0f, 1.0f, 1.0f};
            pines.roughness = 0.85f;

            const auto &xs = f.get_x();
            const auto &ys = f.get_y();
            const auto &zs = f.get_z();
            const auto &rs = f.get_radius();
            pines.instances.reserve(xs.size());
            for (size_t i = 0; i < xs.size(); ++i)
            {
              oros::scene::ObjectInstance inst;
              inst.position = norm_to_world_xy(xs[i], ys[i], zs[i] * z_scale);
              inst.scale = std::max(0.2f, rs[i] * kWorldLx * 0.25f);
              pines.instances.push_back(inst);
            }
            this->p_renderer->add_or_update_instanced_group(pines);
            this->p_renderer->set_instanced_group_visible(
                "main_trees_pine",
                this->get_param_visibility_state("trees"));
          }))
  {
    this->p_renderer->remove_instanced_group("main_trees_pine");
  }

  // 6. Rocks (hmap::RockField)
  if (!helper_try_set_from_port<hmap::RockField>(
          *p_node,
          this->view_param.port_ids.at("rocks"),
          typeid(hmap::RockField),
          [&](const hmap::RockField &r)
          {
            oros::scene::InstancedObjectGroup rocks;
            rocks.id = "main_rocks";
            rocks.mesh_type = oros::scene::ScatterMeshType::ROCK;
            rocks.base_color = {0.72f, 0.70f, 0.68f};
            rocks.roughness = 0.90f;

            const auto &xs = r.get_x();
            const auto &ys = r.get_y();
            const auto &zs = r.get_z();
            const auto &rs = r.get_radius();
            rocks.instances.reserve(xs.size());
            for (size_t i = 0; i < xs.size(); ++i)
            {
              oros::scene::ObjectInstance inst;
              inst.position = norm_to_world_xy(xs[i], ys[i], zs[i] * z_scale);
              inst.scale = std::max(0.2f, rs[i] * kWorldLx * 0.35f);
              rocks.instances.push_back(inst);
            }
            this->p_renderer->add_or_update_instanced_group(rocks);
            this->p_renderer->set_instanced_group_visible(
                "main_rocks",
                this->get_param_visibility_state("rocks"));
          }))
  {
    this->p_renderer->remove_instanced_group("main_rocks");
  }
}

} // namespace hesiod
