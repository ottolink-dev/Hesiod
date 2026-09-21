/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <memory>

#include "nlohmann/json.hpp"

#include "highmap/algebra.hpp"
#include "highmap/virtual_array/virtual_array.hpp"

namespace hesiod
{

struct GraphConfig
{
  // holds the data configuration shared by the graph and its nodes, here is to share for
  // instance the heightmap resolution
  GraphConfig();

  void           json_from(nlohmann::json const &json);
  nlohmann::json json_to() const;

  float       aspect_ratio() const;
  glm::vec2   scale_wavenumber(const glm::vec2 &kw) const;
  std::string info_string() const;
  void        log_debug() const;
  void        set_shape(const glm::ivec2 &new_shape);
  void        set_tiling(const glm::ivec2 &new_tiling);
  void        set_overlap(float new_overlap);

  void update_parameters();

  // --- Members

  glm::ivec2 shape;
  glm::ivec2 tiling;
  float      overlap;

  hmap::ComputeMode cm_gpu = {.mode = hmap::ForEachMode::VA_SINGLE_ARRAY,
                              .trim_storage = false};
  hmap::ComputeMode cm_cpu = {.mode = hmap::ForEachMode::VA_DISTRIBUTED,
                              .trim_storage = false};
  hmap::ComputeMode cm_single_array = {.mode = hmap::ForEachMode::VA_SINGLE_ARRAY,
                                       .trim_storage = false};

  hmap::StorageMode storage_mode = hmap::StorageMode::VA_DISK_LRU;

  // computed from tiling and overlap
  glm::ivec2 tile_shape;
  int        halo;
};

} // namespace hesiod
