/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General Public
 * License. The full license is in the file LICENSE, distributed with this software. */
#pragma once

#include <string>

#include "nlohmann/json.hpp"

namespace hesiod
{

// =====================================
// BakeConfig
// =====================================
struct BakeConfig
{
  int  resolution = 1024;
  int  nvariants = 0;
  bool force_distributed = true;
  bool force_auto_export = true;
  bool rename_export_files = true;
  bool force_maximum_fbm_octaves = false;
  bool min_memory = false;
  int  max_tile_resolution = 32768;

  // where the bake writes; empty: next to the project file (see
  // HesiodApplication::default_bake_dir)
  std::string export_dir;

  void           json_from(nlohmann::json const &json);
  nlohmann::json json_to() const;
};

// =====================================
// Functions
// =====================================
void override_export_nodes_settings(const std::string           &fname,
                                    const std::filesystem::path &export_path,
                                    unsigned int                 random_seeds_increment,
                                    const BakeConfig            &bake_settings);

} // namespace hesiod