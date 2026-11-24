/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
   Public License. The full license is in the file LICENSE, distributed with this
   software. */
#pragma once
#include <array>

#include "highmap/coord_frame.hpp"
#include "highmap/virtual_array/virtual_array.hpp"

#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

class TerrainMergeNode : public BaseNode
{
public:
  struct InputSlot
  {
    const hmap::VirtualArray *p_va = nullptr;
    const hmap::CoordFrame   *p_frame = nullptr;
  };

  TerrainMergeNode(const std::string &label, std::weak_ptr<GraphConfig> config);

  void                            set_input(int                       index,
                                            const hmap::VirtualArray *p_va,
                                            const hmap::CoordFrame   *p_frame);
  const std::array<InputSlot, 4> &get_input_slots() const;

private:
  std::array<InputSlot, 4> input_slots;
};

void setup_terrain_merge_node(BaseNode &node);
void compute_terrain_merge_node(BaseNode &node);

} // namespace hesiod
