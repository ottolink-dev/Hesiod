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
  {
    for (auto &j : value["nodes"])
    {
      const std::string node_label = j.value("label", "");
      const std::string node_id = j.value("id", "");

      auto process_container = [&](nlohmann::json &container)
      {
        // force max octaves
        if (bake_settings.force_maximum_fbm_octaves)
        {
          if (container.contains("octaves"))
          {
            int octaves_max = int(bake_settings.resolution / 128.f); // heuristic...
            if (container["octaves"].is_object() &&
                container["octaves"].contains("value"))
              container["octaves"]["value"] = octaves_max;
            else if (container["octaves"].is_number())
              container["octaves"] = octaves_max;
          }
        }

        // seed increment for variants
        if (random_seeds_increment > 0)
        {
          for (auto &[attr_key, attr_val] : container.items())
          {
            if (attr_key == "seed" ||
                (attr_key.size() > 5 && attr_key.substr(attr_key.size() - 5) == "_seed"))
            {
              if (attr_val.is_object() && attr_val.contains("value") &&
                  attr_val["value"].is_number_integer())
              {
                unsigned int v = attr_val["value"];
                attr_val["value"] = v + random_seeds_increment;
              }
              else if (attr_val.is_number_integer())
              {
                unsigned int v = attr_val;
                attr_val = v + random_seeds_increment;
              }
            }
          }
        }

        // Export nodes tweaking
        if (node_label.find("Export") != std::string::npos ||
            container.contains("auto_export"))
        {
          // force node auto export
          if (bake_settings.force_auto_export)
          {
            if (container.contains("auto_export"))
            {
              if (container["auto_export"].is_object() &&
                  container["auto_export"].contains("value"))
                container["auto_export"]["value"] = true;
              else if (container["auto_export"].is_boolean())
                container["auto_export"] = true;
            }
          }

          // change export name
          if (container.contains("fname"))
          {
            std::string current_fname;
            if (container["fname"].is_object() && container["fname"].contains("value") &&
                container["fname"]["value"].is_string())
            {
              current_fname = container["fname"]["value"].get<std::string>();
            }
            else if (container["fname"].is_string())
            {
              current_fname = container["fname"].get<std::string>();
            }

            if (!current_fname.empty())
            {
              std::filesystem::path basename = std::filesystem::path(current_fname)
                                                   .filename();

              std::string new_name = node_label + "_" + node_id + "_" + basename.string();

              std::filesystem::path new_path = bake_settings.rename_export_files
                                                   ? export_path / new_name
                                                   : export_path / basename;

              if (container["fname"].is_object() && container["fname"].contains("value"))
                container["fname"]["value"] = new_path.string();
              else
                container["fname"] = new_path.string();
            }
          }
        }
      };

      if (j.contains("containers") && j["containers"].is_object())
      {
        for (auto &[cname, cval] : j["containers"].items())
        {
          if (cval.is_object())
            process_container(cval);
        }
      }

      // also check top-level node for legacy format
      process_container(j);
    }
  }

  // write back
  json_to_file(json, fname);
}

} // namespace hesiod
