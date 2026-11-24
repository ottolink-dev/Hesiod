/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/blending.hpp"
#include "highmap/transform/terrain_alignment.hpp"
#include "highmap/virtual_array/virtual_array.hpp"

#include "gnode/graph.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/terrain_merge_node.hpp"

namespace hesiod
{

TerrainMergeNode::TerrainMergeNode(const std::string         &label,
                                   std::weak_ptr<GraphConfig> config)
    : BaseNode(label, config)
{
}

void TerrainMergeNode::set_input(int                       index,
                                 const hmap::VirtualArray *p_va,
                                 const hmap::CoordFrame   *p_frame)
{
  if (index >= 0 && index < 4)
    this->input_slots[index] = {p_va, p_frame};
}

const std::array<TerrainMergeNode::InputSlot, 4> &TerrainMergeNode::get_input_slots()
    const
{
  return this->input_slots;
}

void setup_terrain_merge_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // --- Attributes
  node.set_current_category("Alignment");
  add_bool(node, "align_enabled", "Enable Alignment", true);
  add_enum(
      node,
      "align_mode",
      "Alignment Mode",
      {{"Offset", static_cast<int>(hmap::TerrainAlignMode::TAM_OFFSET)},
       {"Plane", static_cast<int>(hmap::TerrainAlignMode::TAM_PLANE)},
       {"Robust Offset", static_cast<int>(hmap::TerrainAlignMode::TAM_ROBUST_OFFSET)},
       {"Robust Plane", static_cast<int>(hmap::TerrainAlignMode::TAM_ROBUST_PLANE)}},
      "Offset");
  add_float(node, "align_huber_delta", "Huber Delta", 0.1f, 0.001f, 2.f);
  add_int(node, "align_max_iterations", "Max Iterations", 10, 1, 100);
  add_int(node, "align_coarse_size", "Coarse Size", 128, 32, 1024);

  node.set_current_category("Blending");
  add_enum(node,
           "blend_mode",
           "Blending Mode",
           {{"Add", static_cast<int>(hmap::TerrainBlendMode::TBM_ADD)},
            {"Average", static_cast<int>(hmap::TerrainBlendMode::TBM_AVERAGE)},
            {"Distance", static_cast<int>(hmap::TerrainBlendMode::TBM_DISTANCE)},
            {"Height", static_cast<int>(hmap::TerrainBlendMode::TBM_HEIGHT)},
            {"Max", static_cast<int>(hmap::TerrainBlendMode::TBM_MAX)},
            {"Max Smooth", static_cast<int>(hmap::TerrainBlendMode::TBM_MAX_SMOOTH)},
            {"Min", static_cast<int>(hmap::TerrainBlendMode::TBM_MIN)},
            {"Min Smooth", static_cast<int>(hmap::TerrainBlendMode::TBM_MIN_SMOOTH)},
            {"Slope", static_cast<int>(hmap::TerrainBlendMode::TBM_SLOPE)},
            {"Subtract", static_cast<int>(hmap::TerrainBlendMode::TBM_SUBTRACT)}},
           "Distance");
  add_float(node, "blend_param", "Blending Parameter", 0.2f, 0.f, 1.f);
}

void compute_terrain_merge_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto *p_merge = dynamic_cast<TerrainMergeNode *>(&node);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>("output");

  if (!p_merge)
    return;

  const auto *dst_frame = dynamic_cast<const hmap::CoordFrame *>(node.get_p_graph());

  if (!dst_frame)
    return;

  const bool  align_enabled = node.val<bool>("align_enabled");
  const auto  align_mode = node.val_enum<hmap::TerrainAlignMode>("align_mode");
  const float align_huber_delta = node.val<float>("align_huber_delta");
  const int   align_max_iterations = node.val<int>("align_max_iterations");
  const int   align_coarse_size = node.val<int>("align_coarse_size");
  const auto  blend_mode = node.val_enum<hmap::TerrainBlendMode>("blend_mode");
  const float blend_param = node.val<float>("blend_param");

  // zero output before sequential accumulation
  hmap::for_each_tile(
      *p_out,
      [](hmap::Array &array, const hmap::TileRegion &) { array = 0.f; },
      node.cfg().cm_cpu);

  bool has_base = false;

  for (const auto &slot : p_merge->get_input_slots())
  {
    if (!slot.p_va || !slot.p_frame)
      continue;

    if (!has_base)
    {
      hmap::blend_terrains(*p_out,
                           *dst_frame,
                           *slot.p_va,
                           *slot.p_frame,
                           hmap::TerrainBlendMode::TBM_ADD,
                           0.f,
                           node.cfg().cm_cpu);
      has_base = true;
    }
    else if (align_enabled)
    {
      hmap::VirtualArray aligned_upper(slot.p_va->shape,
                                       slot.p_va->tile_shape,
                                       slot.p_va->halo);
      hmap::for_each_tile(
          {slot.p_va},
          {&aligned_upper},
          [](std::vector<const hmap::Array *> p_in_arrays,
             std::vector<hmap::Array *>       p_out_arrays,
             const hmap::TileRegion &) { *p_out_arrays[0] = *p_in_arrays[0]; },
          node.cfg().cm_cpu);

      hmap::align_terrains(*p_out,
                           *dst_frame,
                           aligned_upper,
                           *slot.p_frame,
                           align_mode,
                           align_huber_delta,
                           align_max_iterations,
                           align_coarse_size,
                           node.cfg().cm_cpu);

      hmap::blend_terrains(*p_out,
                           *dst_frame,
                           aligned_upper,
                           *slot.p_frame,
                           blend_mode,
                           blend_param,
                           node.cfg().cm_cpu);
    }
    else
    {
      hmap::blend_terrains(*p_out,
                           *dst_frame,
                           *slot.p_va,
                           *slot.p_frame,
                           blend_mode,
                           blend_param,
                           node.cfg().cm_cpu);
    }
  }
}

} // namespace hesiod
