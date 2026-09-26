/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/model/graph/bake_config.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

void BakeConfig::json_from(nlohmann::json const &json)
{
  json_safe_get(json, "resolution", resolution);
  json_safe_get(json, "nvariants", nvariants);
  json_safe_get(json, "force_distributed", force_distributed);
  json_safe_get(json, "force_auto_export", force_auto_export);
  json_safe_get(json, "rename_export_files", rename_export_files);
  json_safe_get(json, "force_maximum_fbm_octaves", force_maximum_fbm_octaves);
  json_safe_get(json, "min_memory", min_memory);
  json_safe_get(json, "max_tile_resolution", max_tile_resolution);
  json_safe_get(json, "export_dir", export_dir);
}

nlohmann::json BakeConfig::json_to() const
{
  nlohmann::json json;
  json["resolution"] = resolution;
  json["nvariants"] = nvariants;
  json["force_distributed"] = force_distributed;
  json["force_auto_export"] = force_auto_export;
  json["rename_export_files"] = rename_export_files;
  json["force_maximum_fbm_octaves"] = force_maximum_fbm_octaves;
  json["min_memory"] = min_memory;
  json["max_tile_resolution"] = max_tile_resolution;
  json["export_dir"] = export_dir;
  return json;
}

} // namespace hesiod
