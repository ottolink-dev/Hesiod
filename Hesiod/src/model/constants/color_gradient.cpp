/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/model/constants/color_gradient.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

ColorGradientManager::ColorGradientManager()
{
  Logger::log()->info("ColorGradientManager::ColorGradientManager: initializing...");
  this->update_data();
}

std::vector<meta::Preset> ColorGradientManager::get_as_attr_presets() const
{
  std::vector<meta::Preset> presets;

  for (auto &data : this->gradient_data)
  {
    meta::Preset preset;
    preset.name = data.name;
    preset.stops = {};

    for (size_t k = 0; k < data.positions.size(); ++k)
    {
      meta::Stop stop;
      stop.position = data.positions[k];
      stop.color = data.colors[k];
      preset.stops.push_back(stop);
    }

    presets.push_back(preset);
  }

  return presets;
}

void ColorGradientManager::update_data(bool append)
{
  Logger::log()->trace("ColorGradientManager::update_data");

  if (!append)
    this->gradient_data.clear();

  // list files
  const std::string path = HSD_COLOR_GRADIENTS_PATH;
  const std::string extension = ".json";

  try
  {
    for (const auto &entry : std::filesystem::directory_iterator(path))
      if (entry.is_regular_file() && entry.path().extension() == extension)
      {
        const std::string fname = entry.path().string();
        const std::string label = entry.path().filename().replace_extension().string();

        Logger::log()->trace("ColorGradientManager::update_data: parsing {}", fname);

        nlohmann::json json = json_from_file(fname);

        if (json.empty())
        {
          Logger::log()->error(
              "ColorGradientManager::update_data: json parsing error, empty json");
          return;
        }

        ColorGradientData grad;
        grad.name = label;

        for (const auto &grad_json : json["value"])
        {
          if (!grad_json.contains("position") || !grad_json.contains("color"))
          {
            Logger::log()->error("ColorGradientManager::update_data: invalid stop in {}",
                                 grad.name);
            continue;
          }

          grad.positions.push_back(grad_json["position"].get<float>());

          const auto &color_array = grad_json["color"];
          if (!color_array.is_array() || color_array.size() < 4)
          {
            Logger::log()->error("ColorGradientManager::update_data: invalid color in {}",
                                 grad.name);
            continue;
          }

          std::array<float, 4> cf;
          for (size_t k = 0; k < 4; ++k)
            cf[k] = color_array[k].get<float>();

          grad.colors.push_back(std::move(cf));
        }

        this->gradient_data.push_back(std::move(grad));
      }
  }
  catch (const std::filesystem::filesystem_error &e)
  {
    Logger::log()->error("ColorGradientManager::update_data: folder error: {}", e.what());
    return;
  }
}

} // namespace hesiod
