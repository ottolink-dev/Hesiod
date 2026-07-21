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
#include "meta/presets/compat.hpp"
#include "meta/presets/numeric.hpp"

#include "hesiod/logger.hpp"

namespace hsd::compat
{

/// Legacy color-gradient stop (field-compatible with attr::Stop and meta::Stop).
struct Stop
{
  float                position;
  std::array<float, 4> color;
};

// --- tags (template selectors only; never instantiated)
struct FloatAttribute {};
struct IntAttribute {};
struct BoolAttribute {};
struct EnumAttribute {};
struct SeedAttribute {};
struct RangeAttribute {};
struct WaveNbAttribute {};
struct Vec2FloatAttribute {};
struct CloudAttribute {};
struct ColorAttribute {};
struct ColorGradientAttribute {};
struct FilenameAttribute {};
struct StringAttribute {};
struct ChoiceAttribute {};
struct VecFloatAttribute {};

template <typename T> struct legacy_traits; // primary: undefined (unknown tag = compile error)

template <typename T>
concept CompatTag = requires { typename legacy_traits<T>::storage; };

// marker key helpers: record the legacy attribute type so serialization (Task 4)
// can round-trip the original "type_string" and identify seed attributes.
inline void add_compat_markers(meta::AbstractAttribute &a, const char *type_string,
                               bool is_seed = false)
{
  a.metadata().try_add(std::string("compat.legacy_type"), std::string(type_string));
  if (is_seed)
    a.metadata().try_add(std::string("compat.seed"), true);
}

// tolerant field read (legacy json_safe_get parity: warn + keep default)
template <typename V>
inline void safe_get(const nlohmann::json &j, const char *field, V &out,
                     const std::string &key)
{
  if (j.contains(field))
  {
    try
    {
      out = j.at(field).get<V>();
    }
    catch (const std::exception &e)
    {
      hesiod::Logger::log()->warn("compat decode: key '{}' field '{}': {}", key, field,
                                  e.what());
    }
  }
  else
    hesiod::Logger::log()->warn("compat decode: key '{}' missing field '{}', keeping "
                                "default",
                                key, field);
}

inline glm::vec2 vec2_from_json(const nlohmann::json &j, const char *field,
                                glm::vec2 fallback, const std::string &key)
{
  if (j.contains(field) && j.at(field).is_array() && j.at(field).size() == 2 &&
      j.at(field)[0].is_number() && j.at(field)[1].is_number())
    return {j.at(field)[0].get<float>(), j.at(field)[1].get<float>()};
  hesiod::Logger::log()->warn("compat decode: key '{}' bad/missing vec2 field '{}'", key,
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
                                        const std::string &key, const std::string &label,
                                        float value, float vmin = -FLT_MAX,
                                        float vmax = FLT_MAX,
                                        const std::string &value_format = "{:.3f}",
                                        bool log_scale = false)
  {
    auto &a = meta::presets::slider_float(c, key, label, value, vmin, vmax, value_format,
                                          log_scale);
    add_compat_markers(a, type_string);
    return a;
  }

  static legacy_value to_legacy(const storage &v) { return v; }

  static void decode(meta::Attribute<float> &a, const nlohmann::json &j,
                     const std::string &key)
  {
    float v = a.value();
    safe_get(j, "value", v, key);
    a.set_from_any(v); // fires value_changed so any open widgets sync
  }
};

// ---------------------------------------------------------------- Int
template <> struct legacy_traits<IntAttribute>
{
  using storage = int;
  using legacy_value = int;
  static constexpr const char *type_string = "Integer";

  static meta::Attribute<int> &create(meta::AttributeContainer &c, const std::string &key,
                                      const std::string &label, int value,
                                      int vmin = -INT_MAX, int vmax = INT_MAX,
                                      const std::string &value_format = "{}")
  {
    auto &a = meta::presets::slider_int(c, key, label, value, vmin, vmax, value_format);
    add_compat_markers(a, type_string);
    return a;
  }

  static legacy_value to_legacy(const storage &v) { return v; }

  static void decode(meta::Attribute<int> &a, const nlohmann::json &j,
                     const std::string &key)
  {
    int v = a.value();
    safe_get(j, "value", v, key);
    a.set_from_any(v);
  }
};

// ---------------------------------------------------------------- Bool
template <> struct legacy_traits<BoolAttribute>
{
  using storage = bool;
  using legacy_value = bool;
  static constexpr const char *type_string = "Bool";

  static meta::Attribute<bool> &create(meta::AttributeContainer &c,
                                       const std::string &key, const std::string &label,
                                       bool value)
  {
    auto &a = meta::presets::checkbox(c, key, label, value);
    add_compat_markers(a, type_string);
    return a;
  }

  static meta::Attribute<bool> &create(meta::AttributeContainer &c,
                                       const std::string &key, const std::string &label,
                                       const std::string &label_true,
                                       const std::string &label_false, bool value)
  {
    auto &a = meta::presets::binary_buttons(c, key, label, label_true, label_false,
                                            value);
    add_compat_markers(a, type_string);
    return a;
  }

  static legacy_value to_legacy(const storage &v) { return v; }

  static void decode(meta::Attribute<bool> &a, const nlohmann::json &j,
                     const std::string &key)
  {
    bool v = a.value();
    safe_get(j, "value", v, key);
    a.set_from_any(v);
  }
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

  static meta::Attribute<int> &create(meta::AttributeContainer &c,
                                      const std::string &key, const std::string &label,
                                      const std::map<std::string, int> &map)
  {
    const int value = map.begin()->second; // legacy: default = first entry
    auto     &a = meta::presets::enum_choice(c, key, label, make_items(map), value);
    add_compat_markers(a, type_string);
    return a;
  }

  static meta::Attribute<int> &create(meta::AttributeContainer &c,
                                      const std::string &key, const std::string &label,
                                      const std::map<std::string, int> &map,
                                      const std::string &choice)
  {
    int value;
    if (auto it = map.find(choice); it != map.end())
      value = it->second;
    else
    {
      value = map.begin()->second;
      hesiod::Logger::log()->warn(
          "compat enum: key '{}' choice '{}' not in map, defaulting to first entry", key,
          choice);
    }
    auto &a = meta::presets::enum_choice(c, key, label, make_items(map), value);
    add_compat_markers(a, type_string);
    return a;
  }

  static legacy_value to_legacy(const storage &v) { return v; }

  static void decode(meta::Attribute<int> &a, const nlohmann::json &j,
                     const std::string &key)
  {
    const auto *items = a.metadata().try_value<std::vector<std::pair<int, std::string>>>(
        meta::keys::constraints::enum_items);

    // Prefer the "choice" string mapped through enum_items (legacy stored both
    // "choice" and "value"; the string is the stable identity).
    if (j.contains("choice"))
    {
      std::string choice;
      safe_get(j, "choice", choice, key);
      if (items && !items->empty())
      {
        for (const auto &[value, name] : *items)
          if (name == choice)
          {
            a.set_from_any(value);
            return;
          }
        hesiod::Logger::log()->warn(
            "compat enum decode: key '{}' choice '{}' not found, keeping default", key,
            choice);
        return;
      }

      // enum_items metadata unavailable: fall back to the raw "value" int
      // rather than giving up entirely (rare path; enum_items is normally set).
      hesiod::Logger::log()->warn(
          "compat enum decode: key '{}' enum_items unavailable, falling back to raw "
          "'value'",
          key);
      int v = a.value();
      safe_get(j, "value", v, key);
      a.set_from_any(v);
      return;
    }

    // Fallback: raw int value.
    int v = a.value();
    safe_get(j, "value", v, key);
    a.set_from_any(v);
  }
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

  static meta::Attribute<int> &create(meta::AttributeContainer &c, const std::string &key,
                                      const std::string &label, unsigned int value = 0)
  {
    auto &a = meta::presets::seed(c, key, label, static_cast<int>(value));
    add_compat_markers(a, type_string, /*is_seed=*/true);
    return a;
  }

  static legacy_value to_legacy(const storage &v) { return static_cast<unsigned int>(v); }

  static void decode(meta::Attribute<int> &a, const nlohmann::json &j,
                     const std::string &key)
  {
    unsigned int v = static_cast<unsigned int>(a.value());
    safe_get(j, "value", v, key);
    a.set_from_any(static_cast<int>(v));
  }
};

// ---------------------------------------------------------------- Range
template <> struct legacy_traits<RangeAttribute>
{
  using storage = glm::vec2;
  using legacy_value = glm::vec2;
  static constexpr const char *type_string = "Value range";

  static meta::Attribute<glm::vec2> &create(meta::AttributeContainer &c,
                                            const std::string &key)
  {
    return create(c, key, "Range", true); // legacy 0-arg ctor: label "Range"
  }

  static meta::Attribute<glm::vec2> &create(meta::AttributeContainer &c,
                                            const std::string &key,
                                            const std::string &label,
                                            bool               is_active = true)
  {
    auto &a = meta::presets::range(c, key, label, {0.f, 1.f}, -1.f, 2.f, is_active,
                                   "{:.3f}");
    add_compat_markers(a, type_string);
    return a;
  }

  static meta::Attribute<glm::vec2> &create(meta::AttributeContainer &c,
                                            const std::string &key,
                                            const std::string &label, glm::vec2 value,
                                            float vmin, float vmax, bool is_active = true,
                                            const std::string &value_format = "{:.2f}")
  {
    auto &a = meta::presets::range(c, key, label, value, vmin, vmax, is_active,
                                   value_format);
    add_compat_markers(a, type_string);
    return a;
  }

  static legacy_value to_legacy(const storage &v) { return v; }

  static void decode(meta::Attribute<glm::vec2> &a, const nlohmann::json &j,
                     const std::string &key)
  {
    glm::vec2 v = vec2_from_json(j, "value", a.value(), key);
    bool      is_active = true;
    if (const bool *m = a.metadata().try_value<bool>("ui.active"))
      is_active = *m;
    safe_get(j, "is_active", is_active, key);
    // metadata first, then one notify covers the value change
    a.metadata().try_add(std::string("ui.active"), is_active)->value() = is_active;
    a.set_from_any(v);
  }
};

// ---------------------------------------------------------------- WaveNb
template <> struct legacy_traits<WaveNbAttribute>
{
  using storage = glm::vec2;
  using legacy_value = glm::vec2;
  static constexpr const char *type_string = "Wavenumber";

  static meta::Attribute<glm::vec2> &create(meta::AttributeContainer &c,
                                            const std::string &key)
  {
    // legacy 0-arg ctor: label "Wavenumber", value {2,2}, [0, FLT_MAX], link true
    return create(c, key, "Wavenumber", {2.f, 2.f}, 0.f, FLT_MAX, true, "{:.2f}");
  }

  static meta::Attribute<glm::vec2> &create(meta::AttributeContainer &c,
                                            const std::string &key,
                                            const std::string &label)
  {
    // legacy 1-arg ctor: same defaults as 0-arg, custom label
    return create(c, key, label, {2.f, 2.f}, 0.f, FLT_MAX, true, "{:.2f}");
  }

  static meta::Attribute<glm::vec2> &create(meta::AttributeContainer &c,
                                            const std::string &key,
                                            const std::string &label, glm::vec2 value,
                                            float vmin, float vmax, bool link_xy = true,
                                            const std::string &value_format = "{:.2f}")
  {
    auto &a = meta::presets::wavenumber(c, key, label, value, vmin, vmax, link_xy,
                                        value_format);
    add_compat_markers(a, type_string);
    return a;
  }

  static legacy_value to_legacy(const storage &v) { return v; }

  static void decode(meta::Attribute<glm::vec2> &a, const nlohmann::json &j,
                     const std::string &key)
  {
    glm::vec2 v = vec2_from_json(j, "value", a.value(), key);
    bool      link_xy = true;
    if (const bool *m = a.metadata().try_value<bool>("ui.locked_xy"))
      link_xy = *m;
    safe_get(j, "link_xy", link_xy, key);
    a.metadata().try_add(std::string("ui.locked_xy"), link_xy)->value() = link_xy;
    a.set_from_any(v);
  }
};

// ---------------------------------------------------------------- Vec2Float
template <> struct legacy_traits<Vec2FloatAttribute>
{
  using storage = glm::vec2;
  using legacy_value = glm::vec2;
  static constexpr const char *type_string = "Vec2Float";

  static meta::Attribute<glm::vec2> &create(meta::AttributeContainer &c,
                                            const std::string &key,
                                            const std::string &label)
  {
    // legacy 1-arg ctor: value {0.5,0.5}, bounds [0,1]^2
    auto &a = meta::presets::xy(c, key, label, {0.5f, 0.5f}, 0.f, 1.f, 0.f, 1.f);
    add_compat_markers(a, type_string);
    return a;
  }

  static meta::Attribute<glm::vec2> &create(meta::AttributeContainer &c,
                                            const std::string &key,
                                            const std::string &label, glm::vec2 value,
                                            float xmin, float xmax, float ymin,
                                            float ymax)
  {
    auto &a = meta::presets::xy(c, key, label, value, xmin, xmax, ymin, ymax);
    add_compat_markers(a, type_string);
    return a;
  }

  static legacy_value to_legacy(const storage &v) { return v; }

  static void decode(meta::Attribute<glm::vec2> &a, const nlohmann::json &j,
                     const std::string &key)
  {
    glm::vec2 v = vec2_from_json(j, "value", a.value(), key);
    a.set_from_any(v);
  }
};

// ---------------------------------------------------------------- Cloud
template <> struct legacy_traits<CloudAttribute>
{
  using storage = std::vector<glm::vec3>;
  using legacy_value = std::vector<glm::vec3>;
  static constexpr const char *type_string = "Cloud";

  static meta::Attribute<storage> &create(meta::AttributeContainer &c,
                                          const std::string &key, const std::string &label)
  {
    auto &a = meta::presets::points(c, key, label);
    add_compat_markers(a, type_string);
    return a;
  }

  static meta::Attribute<storage> &create(meta::AttributeContainer &c,
                                          const std::string &key,
                                          const std::string &label,
                                          bool               are_points_connected)
  {
    auto &a = meta::presets::points(c, key, label);
    a.metadata().try_add(std::string("ui.closed"), are_points_connected)->value() =
        are_points_connected;
    add_compat_markers(a, type_string);
    return a;
  }

  static meta::Attribute<storage> &create(meta::AttributeContainer &c,
                                          const std::string &key,
                                          const std::string &label, storage value)
  {
    auto &a = meta::presets::points(c, key, label, std::move(value));
    add_compat_markers(a, type_string);
    return a;
  }

  static legacy_value to_legacy(const storage &v) { return v; }

  static void decode(meta::Attribute<storage> &a, const nlohmann::json &j,
                     const std::string &key)
  {
    if (!(j.contains("x") && j.contains("y") && j.contains("values")))
    {
      hesiod::Logger::log()->warn(
          "compat cloud decode: key '{}' missing x/y/values, keeping default", key);
      return;
    }

    std::vector<float> x, y, v;
    try
    {
      x = j.at("x").get<std::vector<float>>();
      y = j.at("y").get<std::vector<float>>();
      v = j.at("values").get<std::vector<float>>();
    }
    catch (const std::exception &e)
    {
      hesiod::Logger::log()->warn("compat cloud decode: key '{}': {}", key, e.what());
      return;
    }

    if (x.size() != y.size() || x.size() != v.size())
    {
      hesiod::Logger::log()->warn(
          "compat cloud decode: key '{}' x/y/values length mismatch, keeping default",
          key);
      return;
    }

    storage points;
    points.reserve(x.size());
    for (size_t k = 0; k < x.size(); ++k)
      points.push_back({x[k], y[k], v[k]});
    a.set_from_any(points);
  }
};

// ---------------------------------------------------------------- Color
template <> struct legacy_traits<ColorAttribute>
{
  using storage = glm::vec4;
  using legacy_value = std::array<float, 4>;
  static constexpr const char *type_string = "Color";

  static meta::Attribute<glm::vec4> &create(meta::AttributeContainer &c,
                                            const std::string &key,
                                            const std::string &label,
                                            const std::array<float, 4> &value)
  {
    auto &a = meta::presets::color(c, key, label,
                                   {value[0], value[1], value[2], value[3]});
    add_compat_markers(a, type_string);
    return a;
  }

  static meta::Attribute<glm::vec4> &create(meta::AttributeContainer &c,
                                            const std::string &key,
                                            const std::string &label, float r, float g,
                                            float b, float a)
  {
    auto &attr = meta::presets::color(c, key, label, {r, g, b, a});
    add_compat_markers(attr, type_string);
    return attr;
  }

  static legacy_value to_legacy(const storage &v) { return {v.x, v.y, v.z, v.w}; }

  static void decode(meta::Attribute<glm::vec4> &a, const nlohmann::json &j,
                     const std::string &key)
  {
    const glm::vec4      cur = a.value();
    std::array<float, 4> arr = {cur.x, cur.y, cur.z, cur.w};
    safe_get(j, "value", arr, key);
    a.set_from_any(glm::vec4(arr[0], arr[1], arr[2], arr[3]));
  }
};

// ---------------------------------------------------------------- ColorGradient
template <> struct legacy_traits<ColorGradientAttribute>
{
  using storage = meta::ColorGradient;
  using legacy_value = std::vector<Stop>;
  static constexpr const char *type_string = "Color gradient";

  static meta::Attribute<meta::ColorGradient> &create(meta::AttributeContainer &c,
                                                      const std::string &key,
                                                      const std::string &label)
  {
    auto &a = meta::presets::color_gradient(c, key, label);
    add_compat_markers(a, type_string);
    return a;
  }

  static meta::Attribute<meta::ColorGradient> &create(meta::AttributeContainer &c,
                                                      const std::string       &key,
                                                      const std::string       &label,
                                                      const std::vector<Stop> &value)
  {
    meta::ColorGradient        g;
    std::vector<meta::Stop>    stops;
    stops.reserve(value.size());
    for (const auto &s : value)
      stops.push_back(meta::Stop{s.position, s.color});
    g.set_value(stops);

    auto &a = meta::presets::color_gradient(c, key, label, g);
    add_compat_markers(a, type_string);
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

  static void decode(meta::Attribute<meta::ColorGradient> &a, const nlohmann::json &j,
                     const std::string &key)
  {
    if (!j.contains("value"))
    {
      hesiod::Logger::log()->warn(
          "compat gradient decode: key '{}' missing value, keeping default", key);
      return;
    }

    std::vector<meta::Stop> stops;
    for (const auto &js : j.at("value"))
    {
      // mirror legacy contains() guards: skip malformed stops
      if (js.contains("position") && js.contains("color"))
      {
        try
        {
          meta::Stop s;
          s.position = js.at("position").get<float>();
          s.color = js.at("color").get<std::array<float, 4>>();
          stops.push_back(s);
        }
        catch (const std::exception &e)
        {
          hesiod::Logger::log()->warn(
              "compat gradient decode: key '{}' malformed stop (position/color): {}", key,
              e.what());
          continue;
        }
      }
    }

    meta::ColorGradient g = a.value();
    g.set_value(stops);
    a.set_from_any(g);
  }
};

// ---------------------------------------------------------------- Filename
template <> struct legacy_traits<FilenameAttribute>
{
  using storage = std::filesystem::path;
  using legacy_value = std::filesystem::path;
  static constexpr const char *type_string = "Filename";

  static meta::Attribute<std::filesystem::path> &create(
      meta::AttributeContainer &c, const std::string &key, const std::string &label,
      const std::filesystem::path &value, const std::string &filter = "",
      bool for_saving = true)
  {
    auto &a = meta::presets::file(c, key, label, value, filter, for_saving);
    add_compat_markers(a, type_string);
    return a;
  }

  static legacy_value to_legacy(const storage &v) { return v; }

  static void decode(meta::Attribute<std::filesystem::path> &a, const nlohmann::json &j,
                     const std::string &key)
  {
    std::string s = a.value().string();
    safe_get(j, "value", s, key);
    a.set_from_any(std::filesystem::path(s));
  }
};

// ---------------------------------------------------------------- String
template <> struct legacy_traits<StringAttribute>
{
  using storage = std::string;
  using legacy_value = std::string;
  static constexpr const char *type_string = "String";

  static meta::Attribute<std::string> &create(meta::AttributeContainer &c,
                                              const std::string &key,
                                              const std::string &label,
                                              const std::string &value)
  {
    auto &a = meta::presets::text(c, key, label, value);
    add_compat_markers(a, type_string);
    return a;
  }

  static meta::Attribute<std::string> &create(meta::AttributeContainer &c,
                                              const std::string &key,
                                              const std::string &label,
                                              const std::string &value, bool read_only)
  {
    auto &a = meta::presets::text(c, key, label, value, read_only);
    add_compat_markers(a, type_string);
    return a;
  }

  static legacy_value to_legacy(const storage &v) { return v; }

  static void decode(meta::Attribute<std::string> &a, const nlohmann::json &j,
                     const std::string &key)
  {
    std::string s = a.value();
    safe_get(j, "value", s, key);
    a.set_from_any(s);
  }
};

// ---------------------------------------------------------------- Choice
template <> struct legacy_traits<ChoiceAttribute>
{
  using storage = std::string;
  using legacy_value = std::string;
  static constexpr const char *type_string = "Choice";

  // legacy (choice_list, value) ctor: label defaults to the key
  static meta::Attribute<std::string> &create(meta::AttributeContainer &c,
                                              const std::string &key,
                                              const std::vector<std::string> &choice_list,
                                              const std::string              &value)
  {
    return create(c, key, key, choice_list, value);
  }

  static meta::Attribute<std::string> &create(meta::AttributeContainer &c,
                                              const std::string &key,
                                              const std::string &label,
                                              const std::vector<std::string> &choice_list,
                                              const std::string              &value)
  {
    auto &a = meta::presets::string_choice(c, key, label, choice_list, value);
    add_compat_markers(a, type_string);
    return a;
  }

  // legacy (label, choice_list) ctor: value = choice_list.front(), throws on empty
  static meta::Attribute<std::string> &create(meta::AttributeContainer &c,
                                              const std::string &key,
                                              const std::string &label,
                                              const std::vector<std::string> &choice_list)
  {
    if (choice_list.empty())
      throw std::invalid_argument("Choice list cannot be empty");
    return create(c, key, label, choice_list, choice_list.front());
  }

  static legacy_value to_legacy(const storage &v) { return v; }

  static void decode(meta::Attribute<std::string> &a, const nlohmann::json &j,
                     const std::string &key)
  {
    std::string s = a.value();
    safe_get(j, "value", s, key);

    if (const auto *allowed = a.metadata().try_value<std::vector<std::string>>(
            meta::keys::constraints::allowed_values))
    {
      if (std::find(allowed->begin(), allowed->end(), s) == allowed->end())
      {
        hesiod::Logger::log()->warn(
            "compat choice decode: key '{}' value '{}' not in list, keeping default", key,
            s);
        return;
      }
    }
    a.set_from_any(s);
  }
};

// ---------------------------------------------------------------- VecFloat
template <> struct legacy_traits<VecFloatAttribute>
{
  using storage = std::vector<float>;
  using legacy_value = std::vector<float>;
  static constexpr const char *type_string = "Vector of floats";

  static meta::Attribute<storage> &create(meta::AttributeContainer &c,
                                          const std::string &key, const std::string &label,
                                          storage value, float vmin, float vmax,
                                          bool /*is_size_variable*/ = true)
  {
    auto &a = meta::presets::curve(c, key, label, std::move(value), vmin, vmax);
    add_compat_markers(a, type_string);
    return a;
  }

  static legacy_value to_legacy(const storage &v) { return v; }

  static void decode(meta::Attribute<storage> &a, const nlohmann::json &j,
                     const std::string &key)
  {
    storage v = a.value();
    safe_get(j, "value", v, key);
    a.set_from_any(v);
  }
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
    if (const auto *m = p_->metadata().try_value<bool>("ui.active"))
      return *m;
    return true;
  }
  void set_is_active(bool v)
  {
    p_->metadata().try_add(std::string("ui.active"), v)->value() = v;
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
  void set_presets(const std::vector<meta::Preset> &presets)
  {
    meta::ColorGradient g = p_->value();
    g.set_presets(presets);
    p_->set_from_any(g);
  }

private:
  meta::Attribute<meta::ColorGradient> *p_;
};

// which handle a tag's get_attr_ref returns
template <typename T> struct handle_of; // undefined by default
template <> struct handle_of<RangeAttribute>    { using type = RangeHandle; };
template <> struct handle_of<ChoiceAttribute>   { using type = ChoiceHandle; };
template <> struct handle_of<StringAttribute>   { using type = StringHandle; };
template <> struct handle_of<FilenameAttribute> { using type = FilenameHandle; };
template <> struct handle_of<BoolAttribute>     { using type = BoolHandle; };
template <> struct handle_of<ColorGradientAttribute> { using type = ColorGradientHandle; };

} // namespace hsd::compat
