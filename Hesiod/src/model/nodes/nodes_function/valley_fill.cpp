/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/primitives.hpp"

#include "highmap/dbg/timer.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_DEPOSITION = "deposition";
constexpr const char *P_IN         = "input";
constexpr const char *P_MASK       = "mask";
constexpr const char *P_NOISE      = "noise";
constexpr const char *P_OUT        = "output";

constexpr const char *A_DURATION                   = "duration";
constexpr const char *A_ELEVATION_MAX_RATIO        = "elevation_max_ratio";
constexpr const char *A_GAMMA                      = "gamma";
constexpr const char *A_PRESERVE_ELEVATION_RANGE   = "preserve_elevation_range";
constexpr const char *A_RATIO                      = "ratio";
constexpr const char *A_SCALE_TALUS_WITH_ELEVATION = "scale_talus_with_elevation";
constexpr const char *A_TALUS_GLOBAL               = "talus_global";

void setup_valley_fill_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_NOISE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_DEPOSITION, CONFIG(node));

  // attribute(s)
  add_float(node, A_TALUS_GLOBAL, "Slope", 1.f, 0.f, FLT_MAX);
  add_float(node, A_DURATION, "Duration", 2.f, 0.05f, 6.f);
  add_float(node, A_RATIO, "Deposition Ratio", 0.8f, 0.f, 1.f);
  add_bool(node, A_PRESERVE_ELEVATION_RANGE, "Preserve Input Range", true);
  add_float(node, A_GAMMA, "Deposition Gamma", 2.f, 0.01f, 4.f);
  add_bool(node, A_SCALE_TALUS_WITH_ELEVATION, "Scale with Elevation", true);
  add_float(node, A_ELEVATION_MAX_RATIO, "Scree Max Elevation", 0.7f, 0.f, 2.f);

  setup_default_noise(node, {.noise_amp = 1.f, .kw = 32.f});
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

void compute_valley_fill_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_out   = node.get_value_ref<hmap::VirtualArray>(P_OUT);
    hmap::VirtualArray *p_noise = node.get_value_ref<hmap::VirtualArray>(P_NOISE);
    hmap::VirtualArray *p_mask  = node.get_value_ref<hmap::VirtualArray>(P_MASK);
    hmap::VirtualArray *p_deposition_map = node.get_value_ref<hmap::VirtualArray>(
        P_DEPOSITION);

    // --- prepapre talus field

    float talus      = node.val<float>(A_TALUS_GLOBAL) / (float)p_out->shape.x;
    int   iterations = int(node.val<float>(A_DURATION) * p_out->shape.x);

    hmap::VirtualArray talus_map = hmap::VirtualArray(CONFIG(node));
    talus_map.fill(talus, node.cfg().cm_cpu);

    if (node.val<bool>(A_SCALE_TALUS_WITH_ELEVATION))
    {
      talus_map.copy_from(*p_in, node.cfg().cm_cpu);
      talus_map.remap(talus / 10.f, talus, node.cfg().cm_cpu);
    }

    // --- generate default noise

    hmap::VirtualArray noise_default(CONFIG(node));
    generate_noise(node, p_noise, noise_default);

    // --- execute

    float zmin = p_in->min(node.cfg().cm_cpu);
    float zmax = p_in->max(node.cfg().cm_cpu);

    hmap::for_each_tile(
        {p_out, p_in, p_noise, p_mask, &talus_map, p_deposition_map},
        [&node, talus, iterations, zmin, zmax](std::vector<hmap::Array *> p_arrays,
                                               const hmap::TileRegion &)
        {
          auto [pa_out,
                pa_in,
                pa_noise,
                pa_mask,
                pa_talus_map,
                pa_deposition_map] = unpack<6>(p_arrays);

          *pa_out = *pa_in;

          hmap::gpu::valley_fill(*pa_out,
                                 pa_mask,
                                 *pa_talus_map,
                                 iterations,
                                 node.val<float>(A_GAMMA),
                                 node.val<float>(A_RATIO),
                                 zmin,
                                 zmax,
                                 node.val<float>(A_ELEVATION_MAX_RATIO),
                                 node.val<bool>(A_PRESERVE_ELEVATION_RANGE),
                                 pa_noise,
                                 pa_deposition_map);
        },
        node.cfg().cm_gpu);

    // post-process
    p_out->smooth_overlap_buffers();
    post_process_heightmap(node, *p_out, p_in);

    p_deposition_map->remap(0.f, 1.f, node.cfg().cm_cpu);
  }
}

} // namespace hesiod
