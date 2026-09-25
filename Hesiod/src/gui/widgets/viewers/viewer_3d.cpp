/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "highmap/geometry/cloud.hpp"
#include "highmap/geometry/path.hpp"
#include "highmap/morphology.hpp"
#include "highmap/range.hpp"

#include "qtr/keys.hpp"
#include "qtr/primitives.hpp"
#include "qtr/render_widget.hpp"
#include "qtr/utils.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/graph_node_widget.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/gui/widgets/icon_check_box.hpp"
#include "hesiod/gui/widgets/viewers/render_helpers.hpp"
#include "hesiod/gui/widgets/viewers/viewer_3d.hpp"
#include "hesiod/gui/widgets/viewers/viewport_controls.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/graph/graph_node.hpp"

namespace hesiod
{

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
  this->setup_layout();
  this->update_widgets();
  this->setup_connections();
  this->update_param_visibility_icons();
}

void Viewer3D::clear()
{
  Viewer::clear();
  if (this->p_renderer)
    this->p_renderer->clear();
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
      // {"trees", ""},
      // {"rocks", ""},
  };

  wp.icons = {
      {"elevation", HSD_ICON("landscape")},
      {"water_depth", HSD_ICON("waves")},
      {"color", HSD_ICON("palette")},
      {"normal_map", HSD_ICON("hdr_strong")},
      {"points", HSD_ICON("scatter_plot")},
      {"path", HSD_ICON("conversion_path")},
  };

  return wp;
}

bool Viewer3D::get_param_visibility_state(const std::string &param_name) const
{
  if (!this->p_renderer)
    return true;

  if (param_name == "elevation")
    return this->p_renderer->is_mesh_visible(qtr::keys::mesh::hmap);
  else if (param_name == "water_depth")
    return this->p_renderer->is_mesh_visible(qtr::keys::mesh::water);
  else if (param_name == "points")
    return this->p_renderer->is_mesh_visible(qtr::keys::mesh::points);
  else if (param_name == "path")
    return this->p_renderer->is_mesh_visible(qtr::keys::mesh::path);
  else if (param_name == "color")
    return !this->p_renderer->get_bypass_texture_albedo();
  else if (param_name == "normal_map")
  {
    // TODO nothing here
  }

  return true;
}

void Viewer3D::json_from(nlohmann::json const &json)
{
  Logger::log()->trace("Viewer3D::json_from");

  Viewer::json_from(json);
  if (p_renderer)
    p_renderer->json_from(json["renderer"]);

  if (this->controls && json.contains("viewport_controls"))
    this->controls->json_from(json["viewport_controls"]);

  this->update_param_visibility_icons();
}

nlohmann::json Viewer3D::json_to() const
{
  Logger::log()->trace("Viewer3D::json_to");

  nlohmann::json json = Viewer::json_to();
  if (p_renderer)
    json["renderer"] = p_renderer->json_to();

  if (this->controls)
    json["viewport_controls"] = this->controls->json_to();

  return json;
}

void Viewer3D::on_view_param_visibility_changed(const std::string &param_name,
                                                bool               new_state)
{
  if (!this->p_renderer)
    return;

  if (param_name == "elevation")
    this->p_renderer->set_mesh_visible(qtr::keys::mesh::hmap, new_state);
  else if (param_name == "water_depth")
    this->p_renderer->set_mesh_visible(qtr::keys::mesh::water, new_state);
  else if (param_name == "points")
    this->p_renderer->set_mesh_visible(qtr::keys::mesh::points, new_state);
  else if (param_name == "path")
    this->p_renderer->set_mesh_visible(qtr::keys::mesh::path, new_state);
  else if (param_name == "color")
    this->p_renderer->set_bypass_texture_albedo(!new_state);
  else if (param_name == "normal_map")
  {
    // TODO nothing here
  }
}

void Viewer3D::resizeEvent(QResizeEvent *)
{
  // nothing floats over the view besides the toolbar, which places itself
}

void Viewer3D::sync_pin_label()
{
  // no node previewed: say so, and there is nothing to pin
  const bool has_node = !this->current_node_id.empty();
  if (!has_node)
    this->button_pin_current_node->set_label("No node previewed");
  this->button_pin_current_node->setEnabled(has_node);
}

void Viewer3D::set_render_type(int new_type)
{
  if (!this->p_renderer)
    return;

  this->p_renderer->set_render_type(new_type == 0 ? qtr::RenderType::RENDER_2D
                                                  : qtr::RenderType::RENDER_3D);
  this->p_renderer->update();

  if (this->controls)
    this->controls->set_render_type(new_type);
}

void Viewer3D::set_skybox(const std::filesystem::path path)
{
  Logger::log()->trace("Viewer3D::set_skybox, path={}", path.string());

  if (!std::filesystem::exists(path))
  {
    Logger::log()->warn("Viewer3D::set_skybox: skybox image filepath does not exist: {}",
                        path.string());
    return;
  }

  try
  {
    int                  sky_width, sky_height;
    std::vector<uint8_t> data = qtr::load_image_as_8bit_rgba(path.string(),
                                                             sky_width,
                                                             sky_height);
    this->p_renderer->set_skybox_image(data, sky_width);
  }
  catch (const std::exception &e)
  {
    Logger::log()->warn("Viewer3D::set_skybox: could not load skybox image {}: {}",
                        path.string(),
                        e.what());
  }
}

void Viewer3D::setup_connections()
{
  Logger::log()->trace("Viewer3D::setup_connections");

  Viewer::setup_connections();

  // add parameters visibility events
  this->connect(this,
                &Viewer::view_param_visibility_changed,
                this,
                &Viewer3D::on_view_param_visibility_changed);
}

void Viewer3D::setup_layout()
{
  Logger::log()->trace("Viewer3D::setup_layout");

  Viewer::setup_layout();

  // retrieve layout and add specific widget
  QGridLayout *grid = dynamic_cast<QGridLayout *>(this->layout());
  int          row_count = get_row_count(grid);

  // add viewer
  this->p_renderer = new qtr::RenderWidget("");
  grid->addWidget(dynamic_cast<QWidget *>(p_renderer), 0, 0, row_count, 1);

  // TODO hardcoded
  this->set_skybox("data/skybox/DaySkyHDRI057B_1K_TONEMAPPED.jpg");

  // the viewport's toolbar and settings panels (replaces the ImGui window)
  this->controls = new ViewportControls(this->p_renderer,
                                        this->p_graph_node_widget,
                                        this);
  this->controls->on_layout_changed = [this]() { this->resizeEvent(nullptr); };

  // Everything about what the view shows lives in the toolbar's Preview
  // panel: first the previewed node with its pin (keeps the view on that node
  // while others are selected), then which output feeds each layer.
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

void Viewer3D::update_renderer()
{
  Logger::log()->trace("Viewer3D::update_renderer");

  // --- early exit cases

  if (!this->p_renderer)
  {
    Logger::log()->error(
        "Viewer3D::update_renderer: renderer reference is a dangling ptr");
    return;
  }

  if (this->current_node_id == "")
  {
    this->p_renderer->clear();
    return;
  }

  BaseNode *p_node = this->safe_get_node();

  if (!p_node)
  {
    this->p_renderer->clear();
    return;
  }

  // --- icons

  this->update_param_visibility_icons();

  // --- route/send data to renderer

  bool flip_y = false;

  if (!helper_try_set_from_port<hmap::VirtualArray>(
          *p_node,
          this->view_param.port_ids.at("elevation"),
          typeid(hmap::VirtualArray),
          [this, p_node](const hmap::VirtualArray &h)
          {
            auto arr = h.to_array(p_node->cfg().cm_cpu);
            if (this->p_renderer)
            {
              bool add_skirt = HSD_CTX.app_settings.viewer.add_heighmap_skirt;

              this->p_renderer->set_heightmap_geometry(arr.vector,
                                                       h.shape.x,
                                                       h.shape.y,
                                                       add_skirt);
            }
          }))
  {
    this->p_renderer->reset_mesh(qtr::keys::mesh::hmap);
  }

  // water
  if (!helper_try_set_from_port<hmap::VirtualArray>(
          *p_node,
          this->view_param.port_ids.at("elevation"),
          this->view_param.port_ids.at("water_depth"),
          typeid(hmap::VirtualArray),
          [this, p_node](const hmap::VirtualArray &h, const hmap::VirtualArray &w)
          {
            // TODO do this somewhere else?
            auto ah = h.to_array(p_node->cfg().cm_cpu); // elevation
            auto aw = w.to_array(p_node->cfg().cm_cpu); // water depth

            // water elevation
            ah += aw;

            // extend the water depth by one-cell to avoid truncated
            // cells at the interface water/ground
            float dh = 1e3f;

            for (int j = 0; j < h.shape.y; ++j)
              for (int i = 0; i < h.shape.x; ++i)
              {
                if (aw(i, j) <= 0.f)
                  ah(i, j) = 0.f;
                else
                  ah(i, j) += dh;
              }

            ah = hmap::dilation_expand_min_value_border_only(ah);

            // remove non-water cells
            float cut_value = -1e3f;

            for (int j = 0; j < h.shape.y; ++j)
              for (int i = 0; i < h.shape.x; ++i)
              {
                if (ah(i, j) <= 0.f)
                  ah(i, j) = cut_value;
                else
                  ah(i, j) -= dh;
              }

            // send to renderer
            if (this->p_renderer)
              this->p_renderer->set_water_geometry(ah.vector,
                                                   h.shape.x,
                                                   h.shape.y,
                                                   cut_value);
          }))
  {
    this->p_renderer->reset_mesh(qtr::keys::mesh::water);
  }

  // color
  if (!(helper_try_set_from_port<hmap::VirtualArray>(
            *p_node,
            this->view_param.port_ids.at("color"),
            typeid(hmap::VirtualArray),
            [this, p_node](const hmap::VirtualArray &h)
            {
              auto arr = h.to_array(p_node->cfg().cm_cpu);
              auto img = generate_selector_image(arr);

              if (this->p_renderer)
                this->p_renderer->set_texture(qtr::keys::tex::albedo, img, h.shape.x);
            }) ||
        helper_try_set_from_port<hmap::VirtualTexture>(
            *p_node,
            this->view_param.port_ids.at("color"),
            typeid(hmap::VirtualTexture),
            [this, p_node, flip_y](const hmap::VirtualTexture &rgba)
            {
              auto img = rgba.to_img_8bit(rgba.shape, p_node->cfg().cm_cpu, flip_y);

              if (this->p_renderer)
                this->p_renderer->set_texture(qtr::keys::tex::albedo, img, rgba.shape.x);
            })))
  {
    this->p_renderer->reset_texture(qtr::keys::tex::albedo);
  }

  // normal map
  if (!helper_try_set_from_port<hmap::VirtualTexture>(
          *p_node,
          this->view_param.port_ids.at("normal_map"),
          typeid(hmap::VirtualTexture),
          [this, p_node, flip_y](const hmap::VirtualTexture &rgba)
          {
            auto img = rgba.to_img_8bit(rgba.shape, p_node->cfg().cm_cpu, flip_y);

            if (this->p_renderer)
              this->p_renderer->set_texture(qtr::keys::tex::normal, img, rgba.shape.x);
          }))
  {
    this->p_renderer->reset_texture(qtr::keys::tex::normal);
  }

  // points
  if (!helper_try_set_from_port<hmap::Cloud>(
          *p_node,
          this->view_param.port_ids.at("points"),
          typeid(hmap::Cloud),
          [this](const hmap::Cloud &c)
          {
            if (this->p_renderer)
              qtr::set_points(*p_renderer, c.get_x(), c.get_y(), c.get_values());
          }))
  {
    this->p_renderer->reset_mesh(qtr::keys::mesh::points);
  }

  // path
  if (!helper_try_set_from_port<hmap::Path>(
          *p_node,
          this->view_param.port_ids.at("path"),
          typeid(hmap::Path),
          [this](const hmap::Path &c)
          {
            if (this->p_renderer)
              qtr::set_path(*p_renderer, c.get_x(), c.get_y(), c.get_values());
          }))
  {
    this->p_renderer->reset_mesh(qtr::keys::mesh::path);
  }
}

} // namespace hesiod
