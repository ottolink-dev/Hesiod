/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <map>
#include <string>
#include <vector>

#include "meta/ext/color_gradient/color_gradient.hpp"

#define HSD_COLOR_GRADIENT_FILE "data/color_gradient.json"
#define HSD_COLOR_GRADIENTS_PATH "data/color_gradients"

namespace hesiod
{

struct ColorGradientData
{
  std::string                       name = "";
  std::vector<float>                positions = {};
  std::vector<std::array<float, 4>> colors = {};
};

class ColorGradientManager
{
public:
  // get the singleton instance
  static ColorGradientManager &get_instance()
  {
    static ColorGradientManager instance;
    return instance;
  }

  std::vector<meta::Preset> get_as_attr_presets() const;
  void                      update_data(bool append = false);

private:
  // private constructor
  ColorGradientManager();

  // delete copy constructor and assignment operator to enforce singleton
  ColorGradientManager(const ColorGradientManager &) = delete;
  ColorGradientManager &operator=(const ColorGradientManager &) = delete;

  // store colormap
  std::vector<ColorGradientData> gradient_data;
};

} // namespace hesiod
