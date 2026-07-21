/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/primitives.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN = "input";
constexpr const char *P_MASK = "mask";
constexpr const char *P_ZMAX = "zmax";
constexpr const char *P_OUT = "output";
constexpr const char *P_DEPOSITION = "deposition";

constexpr const char *A_TALUS_GLOBAL = "talus_global";
constexpr const char *A_ZMAX = "zmax";
constexpr const char *A_DURATION = "duration";
constexpr const char *A_SCALE_TALUS = "scale_talus_with_elevation";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_thermal_scree_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ZMAX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_DEPOSITION, CONFIG(node));

  // --- Attributes

  // clang-format off
  node.add_attr<FloatAttribute>(A_TALUS_GLOBAL, "Slope", 2.f, 0.f, FLT_MAX);
  node.add_attr<FloatAttribute>(A_ZMAX, "Scree Max Elevation", 0.5f, -1.f, 2.f);
  node.add_attr<FloatAttribute>(A_DURATION, "Duration", 0.3f, 0.05f, 6.f);
  node.add_attr<BoolAttribute>(A_SCALE_TALUS, "Scale with Elevation", true);
  // clang-format on

  // --- Attribute(s) order

  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Slope Constraints",
                             A_TALUS_GLOBAL,
                             A_SCALE_TALUS,
                             A_ZMAX,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Deposition Dynamics",
                             A_DURATION,
                             "_GROUPBOX_END_"});

  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_thermal_scree_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  auto *p_zmax = node.get_value_ref<hmap::VirtualArray>(P_ZMAX);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);
  auto *p_deposition = node.get_value_ref<hmap::VirtualArray>(P_DEPOSITION);

  if (!p_in)
    return;

  // --- Params

  // clang-format off
  const auto talus_global = node.get_attr<FloatAttribute>(A_TALUS_GLOBAL);
  const auto zmax_value   = node.get_attr<FloatAttribute>(A_ZMAX);
  const auto duration     = node.get_attr<FloatAttribute>(A_DURATION);
  const auto scale_talus  = node.get_attr<BoolAttribute>(A_SCALE_TALUS);
  // clang-format on

  const float talus = talus_global / float(p_out->shape.x);
  const int   iterations = int(duration * p_out->shape.x);

  // --- Prepare mask

  std::shared_ptr<hmap::VirtualArray> sp_mask = pre_process_mask(node, p_mask, *p_in);

  // --- Talus map

  hmap::VirtualArray talus_map(CONFIG(node));
  talus_map.fill(talus, node.cfg().cm_cpu);

  if (scale_talus)
  {
    talus_map.copy_from(*p_in, node.cfg().cm_cpu);
    talus_map.remap(talus / 10.f, talus, node.cfg().cm_cpu);
  }

  // --- Z max map

  hmap::VirtualArray zmax_map(CONFIG(node));

  if (!p_zmax)
  {
    zmax_map.fill(zmax_value, node.cfg().cm_cpu);
    p_zmax = &zmax_map;
  }

  // --- Compute

  hmap::for_each_tile(
      {p_in, p_mask, &talus_map, p_zmax},
      {p_out, p_deposition},
      [&](std::vector<const hmap::Array *> in,
          std::vector<hmap::Array *>       out,
          const hmap::TileRegion &)
      {
        auto [pa_in, pa_mask, pa_talus_map, pa_zmax] = unpack<4>(in);
        auto [pa_out, pa_deposition] = unpack<2>(out);

        *pa_out = *pa_in;

        hmap::gpu::thermal_scree(*pa_out,
                                 pa_mask,
                                 *pa_talus_map,
                                 *pa_zmax,
                                 iterations,
                                 pa_deposition);
      },
      node.cfg().cm_gpu);

  // --- Post-process

  p_out->smooth_overlap_buffers();
  post_process_heightmap(node, *p_out, p_in);

  if (p_deposition)
  {
    p_deposition->smooth_overlap_buffers();
    p_deposition->remap(0.f, 1.f, node.cfg().cm_cpu);
  }
}

} // namespace hesiod
