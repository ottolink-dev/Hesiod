/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <string>
#include <utility>
#include <vector>

// Settings specs: what each viewport panel shows, and how a UI value maps onto
// the renderer's own (radians, inverted flags, zenith angles...).
namespace hesiod::viewport
{

struct Spec
{
  enum class Type
  {
    Bool,
    Float,
    Color,
    Choice,
    WaterPreset // not a renderer setting: sets both water colours
  };

  Type        type = Type::Float;
  std::string key; // renderer key (and attribute name)
  std::string label;
  std::string category;
  float       vmin = 0.f;
  float       vmax = 1.f;
  std::string format = "{:.2f}";
  bool        angle = false;            // radians in the renderer, degrees here
  bool        invert = false;           // bool shown the other way round
  bool        elevation_zenith = false; // renderer: zenith angle; here: elevation

  std::vector<std::pair<int, std::string>> items; // Choice
};

// the renderer key of a mesh's visibility flag
std::string visible_key(const std::string &mesh);

// the settings a tool's panel shows (empty for tools without a panel)
std::vector<Spec> specs_for(int tool);

} // namespace hesiod::viewport
