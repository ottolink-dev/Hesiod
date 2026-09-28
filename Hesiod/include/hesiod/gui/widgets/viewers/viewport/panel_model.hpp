/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <algorithm>
#include <any>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include <QPointer>

#include "meta/core/attribute_container.hpp"
#include "meta/metadata/keys.hpp"
#include "meta/presets/choice.hpp"
#include "meta/presets/glm.hpp"
#include "meta/presets/numeric.hpp"

#include "qtr/render_widget.hpp"
#include "qtr/water_colors.hpp"

#include "hesiod/gui/widgets/viewers/viewport/viewport_specs.hpp"
#include "hesiod/gui/widgets/viewers/viewport/viewport_style.hpp"

namespace hesiod::viewport
{

// =====================================
// PanelModel: a Meta container mirroring a set of renderer settings
// =====================================

class PanelModel
{
public:
  PanelModel(qtr::RenderWidget *renderer, std::vector<Spec> specs)
      : renderer(renderer), specs(std::move(specs))
  {
    for (const Spec &spec : this->specs)
      this->add(spec);
  }

  // Controls hold subscriptions to the attributes; they must be gone before
  // the container is.
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

  // default of `key` in the attribute's own (UI) type, for the modified state
  std::any default_ui(const std::string &key) const
  {
    const Spec *spec = this->find(key);
    if (!spec || !this->renderer)
      return {};

    if (spec->type == Spec::Type::WaterPreset)
      return this->preset_index_for(glm::vec3(0.25f, 0.85f, 0.80f));

    const std::any raw = this->renderer->default_setting(key);
    if (!raw.has_value())
      return {};

    switch (spec->type)
    {
    case Spec::Type::Bool:
    {
      const bool v = std::any_cast<bool>(raw);
      return spec->invert ? !v : v;
    }
    case Spec::Type::Float:
      return this->to_ui(*spec, std::any_cast<float>(raw));
    case Spec::Type::Color:
      return glm::vec4(std::any_cast<glm::vec3>(raw), 1.f);
    case Spec::Type::Choice:
      return std::any_cast<int>(raw);
    default:
      return {};
    }
  }

  // pull the renderer's current values (they change behind the panel: project
  // load, the orientation gizmo, auto rotation...)
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
        if (bool *p = this->renderer->bool_setting(spec.key))
          this->set_if_changed(this->bools[spec.key], spec.invert ? !*p : *p);
        break;
      case Spec::Type::Float:
        if (float *p = this->renderer->float_setting(spec.key))
          this->set_if_changed(this->floats[spec.key], this->to_ui(spec, *p));
        break;
      case Spec::Type::Color:
        if (glm::vec3 *p = this->renderer->color_setting(spec.key))
          this->set_if_changed(this->colors[spec.key], glm::vec4(*p, 1.f));
        break;
      case Spec::Type::Choice:
        this->set_if_changed(this->ints[spec.key],
                             this->renderer->get_int_setting(spec.key));
        break;
      case Spec::Type::WaterPreset:
        if (glm::vec3 *p = this->renderer->color_setting("color_shallow_water"))
          this->set_if_changed(this->ints[spec.key], this->preset_index_for(*p));
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
      if (spec.type == Spec::Type::WaterPreset)
        continue; // the colours below reset the look already
      if (auto it = this->bools.find(spec.key); it != this->bools.end())
        it->second->set_value(std::any_cast<bool>(v));
      else if (auto it = this->floats.find(spec.key); it != this->floats.end())
        it->second->set_value(std::any_cast<float>(v));
      else if (auto it = this->colors.find(spec.key); it != this->colors.end())
        it->second->set_value(std::any_cast<glm::vec4>(v));
      else if (auto it = this->ints.find(spec.key); it != this->ints.end())
        it->second->set_value(std::any_cast<int>(v));
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

  static float to_ui(const Spec &spec, float value)
  {
    if (!spec.angle)
      return value;
    const float deg = value * 180.f / float(kPi);
    return spec.elevation_zenith ? 90.f - deg : deg;
  }

  static float from_ui(const Spec &spec, float value)
  {
    if (!spec.angle)
      return value;
    const float deg = spec.elevation_zenith ? 90.f - value : value;
    return deg * float(kPi) / 180.f;
  }

  int preset_index_for(const glm::vec3 &shallow) const
  {
    int   i = 0, best = 0;
    float best_d = 1e9f;
    for (const auto &[name, pair] : qtr::water_colors)
    {
      const glm::vec3 d = pair.first - shallow;
      const float     dist = glm::dot(d, d);
      if (dist < best_d)
      {
        best_d = dist;
        best = i;
      }
      ++i;
    }
    return best;
  }

  template <typename T>
  static void set_if_changed(meta::Attribute<T> *attr, const T &value)
  {
    if (attr && !(attr->value() == value))
      attr->set_value(value);
  }

  // floats come back through a unit conversion (degrees <-> radians): only a
  // real change counts, not the rounding of the round trip
  static void set_if_changed(meta::Attribute<float> *attr, const float &value)
  {
    if (attr && std::abs(attr->value() - value) > 1e-4f * std::max(1.f, std::abs(value)))
      attr->set_value(value);
  }

  void add(const Spec &spec)
  {
    auto              &c = this->container;
    qtr::RenderWidget *r = this->renderer;

    const auto tag = [&spec](meta::AbstractAttribute &a)
    {
      a.metadata().try_add(std::string(meta::keys::ui::category),
                           std::string(spec.category));
    };

    switch (spec.type)
    {
    case Spec::Type::Bool:
    {
      bool      *p = r->bool_setting(spec.key);
      const bool value = p ? (spec.invert ? !*p : *p) : false;
      auto      &a = meta::presets::toggle_button(c, spec.key, spec.label, value);
      tag(a);
      this->bools[spec.key] = &a;
      this->connections.push_back(a.value_changed.subscribe(
          [this, spec](const bool &v)
          {
            if (!this->renderer)
              return;
            if (bool *q = this->renderer->bool_setting(spec.key))
            {
              *q = spec.invert ? !v : v;
              this->renderer->settings_changed();
            }
          }));
      break;
    }
    case Spec::Type::Float:
    {
      float      *p = r->float_setting(spec.key);
      const float value = std::clamp(p ? to_ui(spec, *p) : spec.vmin,
                                     spec.vmin,
                                     spec.vmax);
      auto &a = meta::presets::slider_float(c,
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
            if (!this->renderer)
              return;
            if (float *q = this->renderer->float_setting(spec.key))
            {
              *q = from_ui(spec, v);
              this->renderer->settings_changed();
            }
          }));
      break;
    }
    case Spec::Type::Color:
    {
      glm::vec3 *p = r->color_setting(spec.key);
      auto      &a = meta::presets::color(c,
                                     spec.key,
                                     spec.label,
                                     glm::vec4(p ? *p : glm::vec3(1.f), 1.f));
      tag(a);
      this->colors[spec.key] = &a;
      this->connections.push_back(a.value_changed.subscribe(
          [this, spec](const glm::vec4 &v)
          {
            if (!this->renderer)
              return;
            if (glm::vec3 *q = this->renderer->color_setting(spec.key))
            {
              *q = glm::vec3(v);
              this->renderer->settings_changed();
            }
          }));
      break;
    }
    case Spec::Type::Choice:
    {
      auto &a = meta::presets::enum_choice(c,
                                           spec.key,
                                           spec.label,
                                           spec.items,
                                           r->get_int_setting(spec.key));
      tag(a);
      this->ints[spec.key] = &a;
      this->connections.push_back(a.value_changed.subscribe(
          [this, spec](const int &v)
          {
            if (this->renderer)
              this->renderer->set_int_setting(spec.key, v);
          }));
      break;
    }
    case Spec::Type::WaterPreset:
    {
      glm::vec3 *p = r->color_setting("color_shallow_water");
      auto      &a = meta::presets::enum_choice(c,
                                           spec.key,
                                           spec.label,
                                           spec.items,
                                           p ? this->preset_index_for(*p) : 0);
      tag(a);
      this->ints[spec.key] = &a;
      this->connections.push_back(a.value_changed.subscribe(
          [this](const int &v)
          {
            // a sync from the renderer must not re-apply the nearest preset
            // over colours the user picked by hand
            if (this->syncing)
              return;
            int i = 0;
            for (const auto &[name, pair] : qtr::water_colors)
            {
              if (i++ != v)
                continue;
              if (auto it = this->colors.find("color_shallow_water");
                  it != this->colors.end())
                it->second->set_value(glm::vec4(pair.first, 1.f));
              if (auto it = this->colors.find("color_deep_water");
                  it != this->colors.end())
                it->second->set_value(glm::vec4(pair.second, 1.f));
              break;
            }
          }));
      break;
    }
    }
  }

  QPointer<qtr::RenderWidget>                         renderer;
  std::vector<Spec>                                   specs;
  meta::AttributeContainer                            container;
  std::map<std::string, meta::Attribute<bool> *>      bools;
  std::map<std::string, meta::Attribute<float> *>     floats;
  std::map<std::string, meta::Attribute<glm::vec4> *> colors;
  std::map<std::string, meta::Attribute<int> *>       ints;
  std::vector<meta::EventConnection>                  connections;
  bool                                                syncing = false;
};

} // namespace hesiod::viewport
