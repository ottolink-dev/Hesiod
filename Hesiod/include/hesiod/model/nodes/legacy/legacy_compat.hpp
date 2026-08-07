/* Copyright (c) 2024 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */

/**
 * @file legacy_compat.hpp
 * @brief Hesiod->Meta migration compatibility layer.
 *
 * Provides tag structs + `legacy_traits<Tag>` specializations + mutable handles
 * that let the ~300 legacy `attr::`-based nodes keep their
 * `add_attr`/`get_attr`/`get_attr_ref` call sites while routing storage to Meta
 * (`meta::Attribute<T>` + `meta::presets::*`).
 *
 * This layer is DORMANT: nothing includes it yet. Later migration tasks flip
 * node includes from "attributes.hpp" to "compat_attributes.hpp" (the umbrella
 * that re-exports these tags into namespace `attr`).
 *
 * Header-only: every specialization is a template, so there is no companion
 * legacy_compat.cpp.
 */
#pragma once
#include <algorithm>
#include <array>
#include <cfloat>
#include <climits>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "meta/core/attribute.hpp"
#include "meta/core/attribute_container.hpp"
#include "meta/ext/color_gradient/color_gradient.hpp"
#include "meta/metadata/keys.hpp"
#include "meta/presets/choice.hpp"
#include "meta/presets/color_gradient.hpp"
#include "meta/presets/curve.hpp"
#include "meta/presets/file.hpp"
#include "meta/presets/glm.hpp"
#include "meta/presets/numeric.hpp"
#include "meta/presets/text.hpp"

#include "hesiod/logger.hpp"

namespace hsd::legacy
{

/// Legacy color-gradient stop (field-compatible with attr::Stop and meta::Stop).
struct Stop
{
  float                position;
  std::array<float, 4> color;
};

// --- tags (template selectors only; never instantiated)
struct FloatAttribute
{
};
struct IntAttribute
{
};
struct BoolAttribute
{
};
struct EnumAttribute
{
};
struct SeedAttribute
{
};
struct RangeAttribute
{
};
struct WaveNbAttribute
{
};
struct Vec2FloatAttribute
{
};
struct CloudAttribute
{
};
struct ColorAttribute
{
};
struct ColorGradientAttribute
{
};
struct FilenameAttribute
{
};
struct StringAttribute
{
};
struct ChoiceAttribute
{
};
struct VecFloatAttribute
{
};

template <typename T>
struct legacy_traits; // primary: undefined (unknown tag = compile error)

template <typename T>
concept CompatTag = requires { typename legacy_traits<T>::storage; };

inline glm::vec2 vec2_from_json(const nlohmann::json &j,
                                const char           *field,
                                glm::vec2             fallback,
                                const std::string    &key)
{
  if (j.contains(field) && j.at(field).is_array() && j.at(field).size() == 2 &&
      j.at(field)[0].is_number() && j.at(field)[1].is_number())
    return {j.at(field)[0].get<float>(), j.at(field)[1].get<float>()};
  hesiod::Logger::log()->warn("compat decode: key '{}' bad/missing vec2 field '{}'",
                              key,
                              field);
  return fallback;
}

// ---------------------------------------------------------------- Float
template <> struct legacy_traits<FloatAttribute>
{
  using storage = float;
  using legacy_value = float;
  static constexpr const char *type_string = "Float";

  static meta::Attribute<float> &create(meta::AttributeContainer &c,
                                        const std::string        &key,
                                        const std::string        &label,
                                        float                     value,
                                        float                     vmin = -FLT_MAX,
                                        float                     vmax = FLT_MAX,
                                        const std::string        &value_format = "{:.3f}",
                                        bool                      log_scale = false)
  {
    auto &a = meta::presets::slider_float(c,
                                          key,
                                          label,
                                          value,
                                          vmin,
                                          vmax,
                                          value_format,
                                          log_scale);
    return a;
  }

  static legacy_value to_legacy(const storage &v) { return v; }
};

// ---------------------------------------------------------------- Int
template <> struct legacy_traits<IntAttribute>
{
  using storage = int;
  using legacy_value = int;
  static constexpr const char *type_string = "Integer";

  static meta::Attribute<int> &create(meta::AttributeContainer &c,
                                      const std::string        &key,
                                      const std::string        &label,
                                      int                       value,
                                      int                       vmin = -INT_MAX,
                                      int                       vmax = INT_MAX,
                                      const std::string        &value_format = "{}")
  {
    auto &a = meta::presets::slider_int(c, key, label, value, vmin, vmax, value_format);
    return a;
  }

  static legacy_value to_legacy(const storage &v) { return v; }
};

// ---------------------------------------------------------------- Bool
template <> struct legacy_traits<BoolAttribute>
{
  using storage = bool;
  using legacy_value = bool;
  static constexpr const char *type_string = "Bool";

  static meta::Attribute<bool> &create(meta::AttributeContainer &c,
                                       const std::string        &key,
                                       const std::string        &label,
                                       bool                      value)
  {
    auto &a = meta::presets::toggle_button(c, key, label, value);
    return a;
  }

  static meta::Attribute<bool> &create(meta::AttributeContainer &c,
                                       const std::string        &key,
                                       const std::string        &label,
                                       const std::string        &label_true,
                                       const std::string        &label_false,
                                       bool                      value)
  {
    auto &a = meta::presets::binary_buttons(c,
                                            key,
                                            label,
                                            label_true,
                                            label_false,
                                            value);
    return a;
  }

  static legacy_value to_legacy(const storage &v) { return v; }
};

// ---------------------------------------------------------------- Enum
template <> struct legacy_traits<EnumAttribute>
{
  using storage = int;
  using legacy_value = int;
  static constexpr const char *type_string = "Enumeration";

  // Build the (int, string) item list in map (alphabetical) order, matching how
  // the legacy widget presented the choices.
  static std::vector<std::pair<int, std::string>> make_items(
      const std::map<std::string, int> &map)
  {
    std::vector<std::pair<int, std::string>> items;
    items.reserve(map.size());
    for (const auto &[name, value] : map)
      items.emplace_back(value, name);
    return items;
  }

  static meta::Attribute<int> &create(meta::AttributeContainer         &c,
                                      const std::string                &key,
                                      const std::string                &label,
                                      const std::map<std::string, int> &map)
  {
    const int value = map.begin()->second; // legacy: default = first entry
    auto     &a = meta::presets::enum_choice(c, key, label, make_items(map), value);
    return a;
  }

  static meta::Attribute<int> &create(meta::AttributeContainer         &c,
                                      const std::string                &key,
                                      const std::string                &label,
                                      const std::map<std::string, int> &map,
                                      const std::string                &choice)
  {
    int value;
    if (auto it = map.find(choice); it != map.end())
      value = it->second;
    else
    {
      value = map.begin()->second;
      hesiod::Logger::log()->warn(
          "compat enum: key '{}' choice '{}' not in map, defaulting to first entry",
          key,
          choice);
    }
    auto &a = meta::presets::enum_choice(c, key, label, make_items(map), value);
    return a;
  }

  static legacy_value to_legacy(const storage &v) { return v; }
};

// ---------------------------------------------------------------- Seed
template <> struct legacy_traits<SeedAttribute>
{
  using storage = int;
  using legacy_value = unsigned int;
  static constexpr const char *type_string = "Random seed number";

  static meta::Attribute<int> &create(meta::AttributeContainer &c, const std::string &key)
  {
    return create(c, key, "Seed", 0u); // legacy 0-arg ctor: label "Seed", value 0
  }

  static meta::Attribute<int> &create(meta::AttributeContainer &c,
                                      const std::string        &key,
                                      const std::string        &label,
                                      unsigned int              value = 0)
  {
    auto &a = meta::presets::seed(c, key, label, static_cast<int>(value));
    return a;
  }

  static legacy_value to_legacy(const storage &v) { return static_cast<unsigned int>(v); }
};

// ---------------------------------------------------------------- Range
template <> struct legacy_traits<RangeAttribute>
{
  using storage = glm::vec2;
  using legacy_value = glm::vec2;
  static constexpr const char *type_string = "Value range";

  static meta::Attribute<glm::vec2> &create(meta::AttributeContainer &c,
                                            const std::string        &key)
  {
    return create(c, key, "Range", true); // legacy 0-arg ctor: label "Range"
  }

  static meta::Attribute<glm::vec2> &create(meta::AttributeContainer &c,
                                            const std::string        &key,
                                            const std::string        &label,
                                            bool                      is_active = true)
  {
    auto &a = meta::presets::range(c,
                                   key,
                                   label,
                                   {0.f, 1.f},
                                   -1.f,
                                   2.f,
                                   is_active,
                                   "{:.3f}");
    return a;
  }

  static meta::Attribute<glm::vec2> &create(meta::AttributeContainer &c,
                                            const std::string        &key,
                                            const std::string        &label,
                                            glm::vec2                 value,
                                            float                     vmin,
                                            float                     vmax,
                                            bool                      is_active = true,
                                            const std::string &value_format = "{:.2f}")
  {
    auto &a = meta::presets::range(c,
                                   key,
                                   label,
                                   value,
                                   vmin,
                                   vmax,
                                   is_active,
                                   value_format);
    return a;
  }

  static legacy_value to_legacy(const storage &v) { return v; }
};

// ---------------------------------------------------------------- WaveNb
template <> struct legacy_traits<WaveNbAttribute>
{
  using storage = glm::vec2;
  using legacy_value = glm::vec2;
  static constexpr const char *type_string = "Wavenumber";

  static meta::Attribute<glm::vec2> &create(meta::AttributeContainer &c,
                                            const std::string        &key)
  {
    // legacy 0-arg ctor: label "Wavenumber", value {2,2}, [0, FLT_MAX], link true
    return create(c, key, "Wavenumber", {2.f, 2.f}, 0.f, FLT_MAX, true, "{:.2f}");
  }

  static meta::Attribute<glm::vec2> &create(meta::AttributeContainer &c,
                                            const std::string        &key,
                                            const std::string        &label)
  {
    // legacy 1-arg ctor: same defaults as 0-arg, custom label
    return create(c, key, label, {2.f, 2.f}, 0.f, FLT_MAX, true, "{:.2f}");
  }

  static meta::Attribute<glm::vec2> &create(meta::AttributeContainer &c,
                                            const std::string        &key,
                                            const std::string        &label,
                                            glm::vec2                 value,
                                            float                     vmin,
                                            float                     vmax,
                                            bool                      link_xy = true,
                                            const std::string &value_format = "{:.2f}")
  {
    auto &a = meta::presets::wavenumber(c,
                                        key,
                                        label,
                                        value,
                                        vmin,
                                        vmax,
                                        link_xy,
                                        value_format);
    return a;
  }

  static legacy_value to_legacy(const storage &v) { return v; }
};

// ---------------------------------------------------------------- Vec2Float
template <> struct legacy_traits<Vec2FloatAttribute>
{
  using storage = glm::vec2;
  using legacy_value = glm::vec2;
  static constexpr const char *type_string = "Vec2Float";

  static meta::Attribute<glm::vec2> &create(meta::AttributeContainer &c,
                                            const std::string        &key,
                                            const std::string        &label)
  {
    // legacy 1-arg ctor: value {0.5,0.5}, bounds [0,1]^2
    auto &a = meta::presets::xy(c, key, label, {0.5f, 0.5f}, 0.f, 1.f, 0.f, 1.f);
    return a;
  }

  static meta::Attribute<glm::vec2> &create(meta::AttributeContainer &c,
                                            const std::string        &key,
                                            const std::string        &label,
                                            glm::vec2                 value,
                                            float                     xmin,
                                            float                     xmax,
                                            float                     ymin,
                                            float                     ymax)
  {
    auto &a = meta::presets::xy(c, key, label, value, xmin, xmax, ymin, ymax);
    return a;
  }

  static legacy_value to_legacy(const storage &v) { return v; }
};

// ---------------------------------------------------------------- Cloud
template <> struct legacy_traits<CloudAttribute>
{
  using storage = std::vector<glm::vec3>;
  using legacy_value = std::vector<glm::vec3>;
  static constexpr const char *type_string = "Cloud";

  static meta::Attribute<storage> &create(meta::AttributeContainer &c,
                                          const std::string        &key,
                                          const std::string        &label)
  {
    auto &a = meta::presets::points(c, key, label);
    return a;
  }

  static meta::Attribute<storage> &create(meta::AttributeContainer &c,
                                          const std::string        &key,
                                          const std::string        &label,
                                          bool                      are_points_connected)
  {
    auto &a = meta::presets::points(c, key, label);
    a.metadata()
        .try_add(std::string(meta::keys::ui::closed), are_points_connected)
        ->value() = are_points_connected;
    return a;
  }

  static meta::Attribute<storage> &create(meta::AttributeContainer &c,
                                          const std::string        &key,
                                          const std::string        &label,
                                          storage                   value)
  {
    auto &a = meta::presets::points(c, key, label, std::move(value));
    return a;
  }

  static legacy_value to_legacy(const storage &v) { return v; }
};

// ---------------------------------------------------------------- Color
template <> struct legacy_traits<ColorAttribute>
{
  using storage = glm::vec4;
  using legacy_value = std::array<float, 4>;
  static constexpr const char *type_string = "Color";

  static meta::Attribute<glm::vec4> &create(meta::AttributeContainer   &c,
                                            const std::string          &key,
                                            const std::string          &label,
                                            const std::array<float, 4> &value)
  {
    auto &a = meta::presets::color(c,
                                   key,
                                   label,
                                   {value[0], value[1], value[2], value[3]});
    return a;
  }

  static meta::Attribute<glm::vec4> &create(meta::AttributeContainer &c,
                                            const std::string        &key,
                                            const std::string        &label,
                                            float                     r,
                                            float                     g,
                                            float                     b,
                                            float                     a)
  {
    auto &attr = meta::presets::color(c, key, label, {r, g, b, a});
    return attr;
  }

  static legacy_value to_legacy(const storage &v) { return {v.x, v.y, v.z, v.w}; }
};

// ---------------------------------------------------------------- ColorGradient
template <> struct legacy_traits<ColorGradientAttribute>
{
  using storage = meta::ColorGradient;
  using legacy_value = std::vector<Stop>;
  static constexpr const char *type_string = "Color gradient";

  static meta::Attribute<meta::ColorGradient> &create(meta::AttributeContainer &c,
                                                      const std::string        &key,
                                                      const std::string        &label)
  {
    auto &a = meta::presets::color_gradient(c, key, label);
    return a;
  }

  static meta::Attribute<meta::ColorGradient> &create(meta::AttributeContainer &c,
                                                      const std::string        &key,
                                                      const std::string        &label,
                                                      const std::vector<Stop>  &value)
  {
    meta::ColorGradient     g;
    std::vector<meta::Stop> stops;
    stops.reserve(value.size());
    for (const auto &s : value)
      stops.push_back(meta::Stop{s.position, s.color});
    g.set_value(stops);

    auto &a = meta::presets::color_gradient(c, key, label, g);
    return a;
  }

  static legacy_value to_legacy(const storage &v)
  {
    legacy_value out;
    out.reserve(v.value().size());
    for (const auto &s : v.value())
      out.push_back(Stop{s.position, s.color});
    return out;
  }
};

// ---------------------------------------------------------------- Filename
template <> struct legacy_traits<FilenameAttribute>
{
  using storage = std::filesystem::path;
  using legacy_value = std::filesystem::path;
  static constexpr const char *type_string = "Filename";

  static meta::Attribute<std::filesystem::path> &create(
      meta::AttributeContainer    &c,
      const std::string           &key,
      const std::string           &label,
      const std::filesystem::path &value,
      const std::string           &filter = "",
      bool                         for_saving = true)
  {
    auto &a = meta::presets::file(c, key, label, value, filter, for_saving);
    return a;
  }

  static legacy_value to_legacy(const storage &v) { return v; }
};

// ---------------------------------------------------------------- String
template <> struct legacy_traits<StringAttribute>
{
  using storage = std::string;
  using legacy_value = std::string;
  static constexpr const char *type_string = "String";

  static meta::Attribute<std::string> &create(meta::AttributeContainer &c,
                                              const std::string        &key,
                                              const std::string        &label,
                                              const std::string        &value)
  {
    auto &a = meta::presets::text(c, key, label, value);
    return a;
  }

  static meta::Attribute<std::string> &create(meta::AttributeContainer &c,
                                              const std::string        &key,
                                              const std::string        &label,
                                              const std::string        &value,
                                              bool                      read_only)
  {
    auto &a = meta::presets::text(c, key, label, value, read_only);
    return a;
  }

  static legacy_value to_legacy(const storage &v) { return v; }
};

// ---------------------------------------------------------------- Choice
template <> struct legacy_traits<ChoiceAttribute>
{
  using storage = std::string;
  using legacy_value = std::string;
  static constexpr const char *type_string = "Choice";

  // legacy (choice_list, value) ctor: label defaults to the key
  static meta::Attribute<std::string> &create(meta::AttributeContainer       &c,
                                              const std::string              &key,
                                              const std::vector<std::string> &choice_list,
                                              const std::string              &value)
  {
    return create(c, key, key, choice_list, value);
  }

  static meta::Attribute<std::string> &create(meta::AttributeContainer       &c,
                                              const std::string              &key,
                                              const std::string              &label,
                                              const std::vector<std::string> &choice_list,
                                              const std::string              &value)
  {
    auto &a = meta::presets::string_choice(c, key, label, choice_list, value);
    return a;
  }

  // legacy (label, choice_list) ctor: value = choice_list.front(), throws on empty
  static meta::Attribute<std::string> &create(meta::AttributeContainer       &c,
                                              const std::string              &key,
                                              const std::string              &label,
                                              const std::vector<std::string> &choice_list)
  {
    if (choice_list.empty())
      throw std::invalid_argument("Choice list cannot be empty");
    return create(c, key, label, choice_list, choice_list.front());
  }

  static legacy_value to_legacy(const storage &v) { return v; }
};

// ---------------------------------------------------------------- VecFloat
template <> struct legacy_traits<VecFloatAttribute>
{
  using storage = std::vector<float>;
  using legacy_value = std::vector<float>;
  static constexpr const char *type_string = "Vector of floats";

  static meta::Attribute<storage> &create(meta::AttributeContainer &c,
                                          const std::string        &key,
                                          const std::string        &label,
                                          storage                   value,
                                          float                     vmin,
                                          float                     vmax,
                                          bool /*is_size_variable*/ = true)
  {
    auto &a = meta::presets::curve(c, key, label, std::move(value), vmin, vmax);
    return a;
  }

  static legacy_value to_legacy(const storage &v) { return v; }
};

// --- handles: faithful stand-ins for the legacy get_attr_ref<T>() mutable pointers.
// Value semantics + operator-> so `node.get_attr_ref<X>(k)->method()` compiles unchanged.

class RangeHandle
{
public:
  explicit RangeHandle(meta::Attribute<glm::vec2> *p) : p_(p) {}
  RangeHandle *operator->() { return this; }

  bool get_is_active() const
  {
    if (const auto *m = p_->metadata().try_value<bool>(meta::keys::ui::active))
      return *m;
    return true;
  }
  void set_is_active(bool v)
  {
    p_->metadata().try_add(std::string(meta::keys::ui::active), v)->value() = v;
  }
  // Legacy per-attribute reset snapshot. In the Meta model the whole container
  // is snapshotted at finalize_attributes() (after setup, so after any
  // set_is_active() here), which supersedes this per-attribute call: no-op.
  void save_initial_state() {}

private:
  meta::Attribute<glm::vec2> *p_;
};

class ChoiceHandle
{
public:
  explicit ChoiceHandle(meta::Attribute<std::string> *p) : p_(p) {}
  ChoiceHandle *operator->() { return this; }

  std::string get_value() const { return p_->value(); }
  void        set_value(const std::string &v) { p_->set_from_any(v); }
  void        set_use_combo_list(bool combo)
  {
    p_->metadata()
        .try_add(std::string(meta::keys::ui::widget_type),
                 std::string(combo ? "ComboBox" : "ButtonGrid"))
        ->value() = combo ? "ComboBox" : "ButtonGrid";
  }
  void set_choice_list(const std::vector<std::string> &choices)
  {
    p_->metadata()
        .try_add(std::string(meta::keys::constraints::allowed_values), choices)
        ->value() = choices;
  }

private:
  meta::Attribute<std::string> *p_;
};

class StringHandle
{
public:
  explicit StringHandle(meta::Attribute<std::string> *p) : p_(p) {}
  StringHandle *operator->() { return this; }
  void          set_value(const std::string &v) { p_->set_from_any(v); }

private:
  meta::Attribute<std::string> *p_;
};

class FilenameHandle
{
public:
  explicit FilenameHandle(meta::Attribute<std::filesystem::path> *p) : p_(p) {}
  FilenameHandle *operator->() { return this; }
  void            set_value(const std::filesystem::path &v) { p_->set_from_any(v); }

private:
  meta::Attribute<std::filesystem::path> *p_;
};

class BoolHandle
{
public:
  explicit BoolHandle(meta::Attribute<bool> *p) : p_(p) {}
  BoolHandle *operator->() { return this; }
  void        set_value(bool v) { p_->set_from_any(v); }

private:
  meta::Attribute<bool> *p_;
};

class ColorGradientHandle
{
public:
  explicit ColorGradientHandle(meta::Attribute<meta::ColorGradient> *p) : p_(p) {}
  ColorGradientHandle *operator->() { return this; }

  // legacy set_presets: the ColorGradientManager hands meta::Preset lists.
  // Presets are host configuration, not value state: they live in attribute
  // metadata (ui.presets, non-serializable) so a value decode cannot clobber
  // the library installed here.
  void set_presets(const std::vector<meta::Preset> &presets)
  {
    auto &m = p_->metadata();
    if (auto *pp = m.try_value<meta::GradientPresets>(meta::keys::ui::presets))
      pp->presets = presets;
    else
      m.add(meta::keys::ui::presets, meta::GradientPresets{presets});
  }

private:
  meta::Attribute<meta::ColorGradient> *p_;
};

// which handle a tag's get_attr_ref returns
template <typename T> struct handle_of; // undefined by default
template <> struct handle_of<RangeAttribute>
{
  using type = RangeHandle;
};
template <> struct handle_of<ChoiceAttribute>
{
  using type = ChoiceHandle;
};
template <> struct handle_of<StringAttribute>
{
  using type = StringHandle;
};
template <> struct handle_of<FilenameAttribute>
{
  using type = FilenameHandle;
};
template <> struct handle_of<BoolAttribute>
{
  using type = BoolHandle;
};
template <> struct handle_of<ColorGradientAttribute>
{
  using type = ColorGradientHandle;
};

} // namespace hsd::legacy
