/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <filesystem>
#include <random>

#include "hesiod/gui/widgets/graph_config_widgets/bake_config_dialog.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

void override_export_nodes_settings(const std::string           &fname,
                                    const std::filesystem::path &export_path,
                                    unsigned int                 random_seeds_increment,
                                    const BakeConfig            &bake_settings)
{
  Logger::log()->trace("override_export_nodes_settings: fname = {}, export_path = {}",
                       fname,
                       export_path.string());

  std::random_device rd;

  // load
  nlohmann::json json = json_from_file(fname);

  // modify
  for (auto &[key, value] : json["graph_manager"]["graph_nodes"].items())
    for (auto &j : value["nodes"])
    {
      std::string node_label = j["label"];

      // force max octaves
      if (bake_settings.force_maximum_fbm_octaves)
      {
        if (j.contains("octaves") && j["octaves"].contains("value"))
        {
          int octaves_max = int(bake_settings.resolution / 128.f); // heuristic...
          j["octaves"]["value"] = octaves_max;
        }
      }

      // seed increment for variants
      if (random_seeds_increment > 0)
      {
        if (j.contains("seed") && j["seed"].contains("value"))
        {
          unsigned int v = j["seed"]["value"];
          j["seed"]["value"] = v + random_seeds_increment;
        }
      }

      // Export nodes tweaking
      if (node_label.find("Export") != std::string::npos)
      {
        // force node auto export
        if (bake_settings.force_auto_export)
        {
          if (j.contains("auto_export"))
            j["auto_export"]["value"] = true;
        }

        // change export name
        if (j.contains("fname") && j["fname"].contains("value"))
        {
          std::filesystem::path basename = std::filesystem::path(
                                               j["fname"]["value"].get<std::string>())
                                               .filename();

          std::string label = j.value("label", "");
          std::string id = j.value("id", "");

          std::string new_name = label + "_" + id + "_" + basename.string();

          std::filesystem::path new_path = bake_settings.rename_export_files
                                               ? export_path / new_name
                                               : export_path / basename;

          j["fname"]["value"] = new_path.string();
        }
      }
    }

  // write back
  json_to_file(json, fname);
}

} // namespace hesiod
