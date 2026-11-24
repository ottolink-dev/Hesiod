/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <algorithm>
#include <any>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include <QColor>
#include <QPointer>

#include "meta/core/attribute_container.hpp"
#include "meta/metadata/keys.hpp"
#include "meta/presets/choice.hpp"
#include "meta/presets/glm.hpp"
#include "meta/presets/numeric.hpp"

#include "oros/ui/viewport_widget.hpp"

#include "hesiod/gui/widgets/viewers/viewport/viewport_specs.hpp"

namespace hesiod::viewport
{

// =====================================
// PanelModel: a Meta container mirroring a set of Oros viewport settings
// =====================================

class PanelModel
{
public:
  PanelModel(oros::ui::ViewportWidget *renderer, std::vector<Spec> specs)
      : renderer(renderer), specs(std::move(specs))
  {
    for (const Spec &spec : this->specs)
      this->add(spec);
  }

  ~PanelModel() = default;

  meta::AttributeContainer &get_container() { return this->container; }

  meta::Attribute<float> *float_attr(const std::string &key)
  {
    auto it = this->floats.find(key);
    return it == this->floats.end() ? nullptr : it->second;
  }

  std::vector<std::string> categories() const
  {
    std::vector<std::string> out;
    for (const Spec &spec : this->specs)
      if (std::find(out.begin(), out.end(), spec.category) == out.end())
        out.push_back(spec.category);
    return out;
  }

  std::any default_ui(const std::string &key) const
  {
    const Spec *spec = this->find(key);
    if (!spec)
      return {};

    const oros::scene::SunSettings         def_sun{};
    const oros::scene::EnvironmentSettings def_env{};
    const oros::scene::WaterSettings       def_water{};

    if (key == "sun.azimuth_deg")
      return def_sun.azimuth_deg;
    if (key == "sun.elevation_deg")
      return def_sun.elevation_deg;
    if (key == "sun.intensity")
      return def_sun.intensity;
    if (key == "sun.auto_color_temperature")
      return def_sun.auto_color_temperature;
    if (key == "sun.render_sun_disk")
      return def_sun.render_sun_disk;

    if (key == "env.pbr_enabled")
      return def_env.pbr_enabled;
    if (key == "env.soft_shadows_enabled")
      return def_env.soft_shadows_enabled;
    if (key == "env.ambient_occlusion_enabled")
      return def_env.ambient_occlusion_enabled;
    if (key == "env.fog_enabled")
      return def_env.fog_enabled;
    if (key == "env.fog_density")
      return def_env.fog_density;
    if (key == "env.fog_start")
      return def_env.fog_start;
    if (key == "env.fog_height")
      return def_env.fog_height;
    if (key == "env.fog_height_falloff")
      return def_env.fog_height_falloff;
    if (key == "env.atmospheric_scattering_enabled")
      return def_env.atmospheric_scattering_enabled;
    if (key == "env.atmosphere_turbidity")
      return def_env.atmosphere_turbidity;
    if (key == "env.rayleigh_strength")
      return def_env.rayleigh_strength;
    if (key == "env.mie_strength")
      return def_env.mie_strength;
    if (key == "env.ground_plane_enabled")
      return def_env.ground_plane_enabled;
    if (key == "env.ground_plane_elevation")
      return def_env.ground_plane_elevation;
    if (key == "env.ground_plane_color")
      return glm::vec4(def_env.ground_plane_color.x,
                       def_env.ground_plane_color.y,
                       def_env.ground_plane_color.z,
                       1.f);

    if (key == "water.shallow_color")
      return glm::vec4(def_water.shallow_color.x,
                       def_water.shallow_color.y,
                       def_water.shallow_color.z,
                       1.f);
    if (key == "water.deep_color")
      return glm::vec4(def_water.deep_color.x,
                       def_water.deep_color.y,
                       def_water.deep_color.z,
                       1.f);
    if (key == "water.depth_falloff")
      return def_water.depth_falloff;
    if (key == "water.opacity")
      return def_water.opacity;
    if (key == "water.roughness")
      return def_water.roughness;

    if (key == "cam.fov_deg")
      return 45.0f;
    if (key == "terrain.height_scale_multiplier")
      return 1.0f;
    if (key == "cdlod.wireframe_enabled")
      return false;
    if (key == "cdlod.morphing_enabled")
      return true;
    if (key == "cdlod.lod_color_debug_enabled")
      return false;
    if (key == "cdlod.lod_distance_scale")
      return 1.0f;
    if (key == "cdlod.shading_enabled")
      return true;
    if (key == "view.background_color")
      return glm::vec4(30.f / 255.f, 30.f / 255.f, 35.f / 255.f, 1.f);

    return {};
  }

  void sync_from_renderer()
  {
    if (!this->renderer)
      return;

    this->syncing = true;
    for (const Spec &spec : this->specs)
    {
      switch (spec.type)
      {
      case Spec::Type::Bool:
        this->set_if_changed(this->bools[spec.key], this->read_bool(spec.key));
        break;
      case Spec::Type::Float:
        this->set_if_changed(this->floats[spec.key], this->read_float(spec.key));
        break;
      case Spec::Type::Color:
        this->set_if_changed(this->colors[spec.key], this->read_color(spec.key));
        break;
      default:
        break;
      }
    }
    this->syncing = false;
  }

  void reset_to_defaults()
  {
    for (const Spec &spec : this->specs)
    {
      const std::any v = this->default_ui(spec.key);
      if (!v.has_value())
        continue;
      if (auto it = this->bools.find(spec.key); it != this->bools.end())
        it->second->set_value(std::any_cast<bool>(v));
      else if (auto it = this->floats.find(spec.key); it != this->floats.end())
        it->second->set_value(std::any_cast<float>(v));
      else if (auto it = this->colors.find(spec.key); it != this->colors.end())
        it->second->set_value(std::any_cast<glm::vec4>(v));
    }
  }

private:
  const Spec *find(const std::string &key) const
  {
    for (const Spec &spec : this->specs)
      if (spec.key == key)
        return &spec;
    return nullptr;
  }

  bool read_bool(const std::string &key) const
  {
    if (!this->renderer)
      return false;
    const auto &sun = this->renderer->get_sun_settings();
    const auto &env = this->renderer->get_environment_settings();

    if (key == "sun.auto_color_temperature")
      return sun.auto_color_temperature;
    if (key == "sun.render_sun_disk")
      return sun.render_sun_disk;
    if (key == "env.pbr_enabled")
      return env.pbr_enabled;
    if (key == "env.soft_shadows_enabled")
      return env.soft_shadows_enabled;
    if (key == "env.ambient_occlusion_enabled")
      return env.ambient_occlusion_enabled;
    if (key == "env.fog_enabled")
      return env.fog_enabled;
    if (key == "env.atmospheric_scattering_enabled")
      return env.atmospheric_scattering_enabled;
    if (key == "env.ground_plane_enabled")
      return env.ground_plane_enabled;
    return false;
  }

  float read_float(const std::string &key) const
  {
    if (!this->renderer)
      return 0.0f;
    const auto &sun = this->renderer->get_sun_settings();
    const auto &env = this->renderer->get_environment_settings();
    const auto &wat = this->renderer->get_water_settings();

    if (key == "sun.azimuth_deg")
      return sun.azimuth_deg;
    if (key == "sun.elevation_deg")
      return sun.elevation_deg;
    if (key == "sun.intensity")
      return sun.intensity;
    if (key == "cam.fov_deg")
      return this->renderer->get_fov_deg();
    if (key == "terrain.height_scale_multiplier")
      return this->renderer->get_height_scale_multiplier();
    if (key == "env.ground_plane_elevation")
      return env.ground_plane_elevation;
    if (key == "cdlod.lod_distance_scale")
      return this->renderer->get_lod_distance_scale();
    if (key == "water.depth_falloff")
      return wat.depth_falloff;
    if (key == "water.opacity")
      return wat.opacity;
    if (key == "water.roughness")
      return wat.roughness;
    if (key == "env.atmosphere_turbidity")
      return env.atmosphere_turbidity;
    if (key == "env.rayleigh_strength")
      return env.rayleigh_strength;
    if (key == "env.mie_strength")
      return env.mie_strength;
    if (key == "env.fog_density")
      return env.fog_density;
    if (key == "env.fog_start")
      return env.fog_start;
    if (key == "env.fog_height")
      return env.fog_height;
    if (key == "env.fog_height_falloff")
      return env.fog_height_falloff;
    return 0.0f;
  }

  glm::vec4 read_color(const std::string &key) const
  {
    if (!this->renderer)
      return glm::vec4(1.f);
    const auto &env = this->renderer->get_environment_settings();
    const auto &wat = this->renderer->get_water_settings();

    if (key == "env.ground_plane_color")
      return glm::vec4(env.ground_plane_color.x,
                       env.ground_plane_color.y,
                       env.ground_plane_color.z,
                       1.f);
    if (key == "water.shallow_color")
      return glm::vec4(wat.shallow_color.x,
                       wat.shallow_color.y,
                       wat.shallow_color.z,
                       1.f);
    if (key == "water.deep_color")
      return glm::vec4(wat.deep_color.x, wat.deep_color.y, wat.deep_color.z, 1.f);
    if (key == "view.background_color")
    {
      const QColor &c = this->renderer->get_background_color();
      return glm::vec4(c.redF(), c.greenF(), c.blueF(), 1.f);
    }
    return glm::vec4(1.f);
  }

  void write_bool(const std::string &key, bool v)
  {
    if (!this->renderer)
      return;
    if (key == "sun.auto_color_temperature")
    {
      auto s = this->renderer->get_sun_settings();
      s.auto_color_temperature = v;
      this->renderer->set_sun_settings(s);
    }
    else if (key == "sun.render_sun_disk")
    {
      auto s = this->renderer->get_sun_settings();
      s.render_sun_disk = v;
      this->renderer->set_sun_settings(s);
    }
    else if (key == "env.pbr_enabled")
      this->renderer->set_pbr_environment_enabled(v);
    else if (key == "env.soft_shadows_enabled")
      this->renderer->set_soft_shadows_enabled(v);
    else if (key == "env.ambient_occlusion_enabled")
    {
      auto e = this->renderer->get_environment_settings();
      e.ambient_occlusion_enabled = v;
      this->renderer->set_environment_settings(e);
    }
    else if (key == "env.fog_enabled")
      this->renderer->set_fog_enabled(v);
    else if (key == "env.atmospheric_scattering_enabled")
      this->renderer->set_atmospheric_scattering_enabled(v);
    else if (key == "env.ground_plane_enabled")
      this->renderer->set_ground_plane_enabled(v);
    else if (key == "cdlod.wireframe_enabled")
      this->renderer->set_wireframe_enabled(v);
    else if (key == "cdlod.morphing_enabled")
      this->renderer->set_morphing_enabled(v);
    else if (key == "cdlod.lod_color_debug_enabled")
      this->renderer->set_lod_color_debug_enabled(v);
    else if (key == "cdlod.shading_enabled")
      this->renderer->set_shading_enabled(v);
  }

  void write_float(const std::string &key, float v)
  {
    if (!this->renderer)
      return;
    if (key == "sun.azimuth_deg" || key == "sun.elevation_deg" || key == "sun.intensity")
    {
      auto s = this->renderer->get_sun_settings();
      if (key == "sun.azimuth_deg")
        s.azimuth_deg = v;
      else if (key == "sun.elevation_deg")
        s.elevation_deg = v;
      else
        s.intensity = v;
      this->renderer->set_sun_settings(s);
    }
    else if (key == "cam.fov_deg")
      this->renderer->set_fov_deg(v);
    else if (key == "terrain.height_scale_multiplier")
      this->renderer->set_height_scale_multiplier(v);
    else if (key == "env.ground_plane_elevation")
      this->renderer->set_ground_plane_elevation(v);
    else if (key == "cdlod.lod_distance_scale")
      this->renderer->set_lod_distance_scale(v);
    else if (key == "water.depth_falloff" || key == "water.opacity" ||
             key == "water.roughness")
    {
      auto w = this->renderer->get_water_settings();
      if (key == "water.depth_falloff")
        w.depth_falloff = v;
      else if (key == "water.opacity")
        w.opacity = v;
      else
        w.roughness = v;
      this->renderer->set_water_settings(w);
    }
    else if (key.rfind("env.", 0) == 0)
    {
      auto e = this->renderer->get_environment_settings();
      if (key == "env.atmosphere_turbidity")
        e.atmosphere_turbidity = v;
      else if (key == "env.rayleigh_strength")
        e.rayleigh_strength = v;
      else if (key == "env.mie_strength")
        e.mie_strength = v;
      else if (key == "env.fog_density")
        e.fog_density = v;
      else if (key == "env.fog_start")
        e.fog_start = v;
      else if (key == "env.fog_height")
        e.fog_height = v;
      else if (key == "env.fog_height_falloff")
        e.fog_height_falloff = v;
      this->renderer->set_environment_settings(e);
    }
  }

  void write_color(const std::string &key, const glm::vec4 &v)
  {
    if (!this->renderer)
      return;
    if (key == "env.ground_plane_color")
    {
      auto e = this->renderer->get_environment_settings();
      e.ground_plane_color = {v.r, v.g, v.b};
      this->renderer->set_environment_settings(e);
    }
    else if (key == "water.shallow_color" || key == "water.deep_color")
    {
      auto w = this->renderer->get_water_settings();
      if (key == "water.shallow_color")
        w.shallow_color = {v.r, v.g, v.b};
      else
        w.deep_color = {v.r, v.g, v.b};
      this->renderer->set_water_settings(w);
    }
    else if (key == "view.background_color")
    {
      this->renderer->set_background_color(QColor::fromRgbF(v.r, v.g, v.b, 1.0f));
    }
  }

  template <typename T>
  static void set_if_changed(meta::Attribute<T> *attr, const T &value)
  {
    if (attr && !(attr->value() == value))
      attr->set_value(value);
  }

  static void set_if_changed(meta::Attribute<float> *attr, const float &value)
  {
    if (attr && std::abs(attr->value() - value) > 1e-4f * std::max(1.f, std::abs(value)))
      attr->set_value(value);
  }

  void add(const Spec &spec)
  {
    auto &c = this->container;

    const auto tag = [&spec](meta::AbstractAttribute &a)
    {
      a.metadata().try_add(std::string(meta::keys::ui::category),
                           std::string(spec.category));
    };

    switch (spec.type)
    {
    case Spec::Type::Bool:
    {
      const bool value = this->read_bool(spec.key);
      auto      &a = meta::presets::toggle_button(c, spec.key, spec.label, value);
      tag(a);
      this->bools[spec.key] = &a;
      this->connections.push_back(a.value_changed.subscribe(
          [this, spec](const bool &v)
          {
            if (!this->syncing)
              this->write_bool(spec.key, v);
          }));
      break;
    }
    case Spec::Type::Float:
    {
      const float value = std::clamp(this->read_float(spec.key), spec.vmin, spec.vmax);
      auto       &a = meta::presets::slider_float(c,
                                            spec.key,
                                            spec.label,
                                            value,
                                            spec.vmin,
                                            spec.vmax,
                                            spec.format);
      tag(a);
      this->floats[spec.key] = &a;
      this->connections.push_back(a.value_changed.subscribe(
          [this, spec](const float &v)
          {
            if (!this->syncing)
              this->write_float(spec.key, v);
          }));
      break;
    }
    case Spec::Type::Color:
    {
      const glm::vec4 value = this->read_color(spec.key);
      auto           &a = meta::presets::color(c, spec.key, spec.label, value);
      tag(a);
      this->colors[spec.key] = &a;
      this->connections.push_back(a.value_changed.subscribe(
          [this, spec](const glm::vec4 &v)
          {
            if (!this->syncing)
              this->write_color(spec.key, v);
          }));
      break;
    }
    default:
      break;
    }
  }

  QPointer<oros::ui::ViewportWidget>                  renderer;
  std::vector<Spec>                                   specs;
  meta::AttributeContainer                            container;
  std::map<std::string, meta::Attribute<bool> *>      bools;
  std::map<std::string, meta::Attribute<float> *>     floats;
  std::map<std::string, meta::Attribute<glm::vec4> *> colors;
  std::vector<meta::EventConnection>                  connections;
  bool                                                syncing = false;
};

} // namespace hesiod::viewport
