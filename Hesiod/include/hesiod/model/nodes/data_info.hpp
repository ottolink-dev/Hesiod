/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <string>

#include "highmap/array.hpp"
#include "highmap/flora/forest.hpp"
#include "highmap/geometry/cloud.hpp"
#include "highmap/geometry/path.hpp"
#include "highmap/virtual_array/virtual_array.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

template <typename T> std::string get_data_info(BaseNode *node, const std::string &key)
{
  auto *p_val = node->get_value_ref<T>(key);
  if (!p_val)
    return "-";

  if constexpr (std::is_same_v<T, hmap::VirtualArray>)
  {
    return std::format("[{:.2e}, {:.2e}]",
                       p_val->min(node->cfg().cm_cpu),
                       p_val->max(node->cfg().cm_cpu));
  }
  else if constexpr (std::is_same_v<T, hmap::VirtualTexture>)
  {
    return std::format("R: [{:.2e}, {:.2e}], G: [{:.2e}, {:.2e}], B: [{:.2e}, {:.2e}], "
                       "A: [{:.2e}, {:.2e}]",
                       p_val->channel(0).min(node->cfg().cm_cpu),
                       p_val->channel(0).max(node->cfg().cm_cpu),
                       p_val->channel(1).min(node->cfg().cm_cpu),
                       p_val->channel(1).max(node->cfg().cm_cpu),
                       p_val->channel(2).min(node->cfg().cm_cpu),
                       p_val->channel(2).max(node->cfg().cm_cpu),
                       p_val->channel(3).min(node->cfg().cm_cpu),
                       p_val->channel(3).max(node->cfg().cm_cpu));
  }
  else if constexpr (std::is_same_v<T, hmap::Array>)
  {
    return std::format("[{:.2e}, {:.2e}]", p_val->min(), p_val->max());
  }
  else if constexpr (std::is_same_v<T, hmap::Cloud> || std::is_same_v<T, hmap::Path>)
  {
    return std::format("[{:.2e}, {:.2e}], count: {}",
                       p_val->get_values_min(),
                       p_val->get_values_max(),
                       p_val->size());
  }
  else if constexpr (std::is_same_v<T, hmap::Forest>)
  {
    return std::format("trees: {}", p_val->size());
  }
  else
  {
    return "-";
  }
}

} // namespace hesiod
