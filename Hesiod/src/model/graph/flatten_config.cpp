/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <iostream>

#include "hesiod/model/graph/flatten_config.hpp"
#include "hesiod/model/utils.hpp"
#include "logger/logger.hpp"

namespace hesiod
{

void FlattenConfig::dump()
{
  LOG_TRACE("FlattenConfig::dump");
  std::cout << this->json_to().dump(4) << "\n";
}

void FlattenConfig::json_from(nlohmann::json const &json)
{
  json_safe_get(json, "shape.x", this->shape.x);
  json_safe_get(json, "shape.y", this->shape.y);
  json_safe_get(json, "tiling.x", this->tiling.x);
  json_safe_get(json, "tiling.y", this->tiling.y);

  json_safe_get(json, "overlap", this->overlap);
  json_safe_get(json, "export_path", this->export_path);

  json_safe_get(json, "ids", this->ids);
}

nlohmann::json FlattenConfig::json_to() const
{
  nlohmann::json json;
  json["shape.x"] = this->shape.x;
  json["shape.y"] = this->shape.y;
  json["tiling.x"] = this->tiling.x;
  json["tiling.y"] = this->tiling.y;
  json["overlap"] = this->overlap;
  json["export_path"] = this->export_path;
  json["ids"] = this->ids;
  return json;
}

} // namespace hesiod
