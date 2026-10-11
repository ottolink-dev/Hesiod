/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <string>

#include <QApplication>
#include <QColor>

#include "highmap/array.hpp"
#include "highmap/flora/forest.hpp"
#include "highmap/geometry/cloud.hpp"
#include "highmap/geometry/path.hpp"
#include "highmap/virtual_array/virtual_array.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "nlohmann/json.hpp"

namespace hesiod
{

struct StyleSettings
{
  StyleSettings() = default;

  // --- Serialization
  void           json_from(nlohmann::json const &json);
  nlohmann::json json_to() const;

  // --- Data
  std::map<std::string, QColor> data_color_map = {
      {typeid(hmap::Array).name(), QColor(255, 121, 198, 255)},
      {typeid(hmap::Cloud).name(), QColor(139, 233, 253, 255)},
      {typeid(hmap::Forest).name(), QColor(106, 168, 79, 255)},
      {typeid(hmap::VirtualArray).name(), QColor(248, 248, 242, 255)},
      {typeid(hmap::VirtualTexture).name(), QColor(189, 147, 249, 255)},
      {typeid(hmap::Path).name(), QColor(255, 184, 108, 255)},
      {typeid(std::vector<float>).name(), QColor(98, 114, 164, 255)},
  };

  std::map<std::string, QColor> category_color_map = {
      {"Converter", QColor(188, 182, 163, 255)}, {"Comment", QColor(170, 170, 170, 255)},
      {"Debug", QColor(200, 0, 0, 255)},         {"Math", QColor(0, 43, 54, 255)},
      {"Geometry", QColor(101, 123, 131, 255)},  {"Flora", QColor(104, 159, 56, 255)},
      {"Roads", QColor(147, 161, 161, 255)},     {"Routing", QColor(188, 182, 163, 255)},
      {"IO", QColor(203, 196, 177, 255)},        {"Features", QColor(181, 137, 0, 255)},
      {"Erosion", QColor(203, 75, 22, 255)},     {"Mask", QColor(211, 54, 130, 255)},
      {"Filter", QColor(108, 113, 196, 255)},    {"Operator", QColor(108, 113, 196, 255)},
      {"Hydrology", QColor(38, 139, 210, 255)},  {"Primitive", QColor(42, 161, 152, 255)},
      {"Biomes", QColor(133, 153, 0, 255)},      {"Texture", QColor(0, 0, 0, 255)},
      {"WIP", QColor(255, 255, 255, 255)},
  };
};

} // namespace hesiod