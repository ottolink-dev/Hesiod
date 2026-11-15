/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/gui/theme/node_graph_color_palette.hpp"
#include "hesiod/model/utils.hpp"
#include "logger/logger.hpp"

namespace hesiod
{

void NodeGraphColorPalette::json_from(nlohmann::json const &json)
{
  LOG_TRACE("NodeGraphColorPalette::json_from");

  if (json.contains("data_color_map"))
  {
    for (auto &[k, v] : json["data_color_map"].items())
      this->data_color_map[k] = QColor(std::string(v).c_str());
  }

  if (json.contains("category_color_map"))
  {
    for (auto &[k, v] : json["category_color_map"].items())
      this->category_color_map[k] = QColor(std::string(v).c_str());
  }
}

nlohmann::json NodeGraphColorPalette::json_to() const
{
  LOG_TRACE("NodeGraphColorPalette::json_to");

  nlohmann::json json;

  {
    nlohmann::json j;
    for (auto &[k, v] : this->data_color_map)
      j[k] = v.name().toStdString();
    json["data_color_map"] = j;
  }

  {
    nlohmann::json j;
    for (auto &[k, v] : this->category_color_map)
      j[k] = v.name().toStdString();
    json["category_color_map"] = j;
  }

  return json;
}

} // namespace hesiod
