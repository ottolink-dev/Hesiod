/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"
#include "highmap/primitives.hpp"

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
constexpr const char *P_OUT        = "output";

constexpr const char *A_DURATION                   = "duration";
constexpr const char *A_MAX_DEPOSITION             = "max_deposition";
constexpr const char *A_SCALE_TALUS_WITH_ELEVATION = "scale_talus_with_elevation";
constexpr const char *A_SUBITERATIONS              = "subiterations";
constexpr const char *A_TALUS_GLOBAL               = "talus_global";

void setup_sediment_deposition_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_DEPOSITION, CONFIG(node));

  // attribute(s)
  add_float(node, A_TALUS_GLOBAL, "talus_global", 0.2f, 0.f, FLT_MAX);
  add_float(node, A_MAX_DEPOSITION, "max_deposition", 0.001f, 0.f, 0.1f);
  add_float(node, A_DURATION, "Duration", 0.3f, 0.05f, 6.f);
  add_bool(node, A_SCALE_TALUS_WITH_ELEVATION, "scale_talus_with_elevation", true);
  add_int(node, A_SUBITERATIONS, "subiterations", 10, 1, 50);
}

void compute_sediment_deposition_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_mask           = node.get_value_ref<hmap::VirtualArray>(P_MASK);
    hmap::VirtualArray *p_out            = node.get_value_ref<hmap::VirtualArray>(P_OUT);
    hmap::VirtualArray *p_deposition_map = node.get_value_ref<hmap::VirtualArray>(
        P_DEPOSITION);

    float talus = node.val<float>(A_TALUS_GLOBAL) / (float)p_out->shape.x;

    hmap::VirtualArray talus_map = hmap::VirtualArray(CONFIG(node));
    talus_map.fill(talus, node.cfg().cm_cpu);

    if (node.val<bool>(A_SCALE_TALUS_WITH_ELEVATION))
    {
      talus_map.copy_from(*p_in, node.cfg().cm_cpu);
      talus_map.remap(talus / 100.f, talus, node.cfg().cm_cpu);
    }

    int iterations = int(node.val<float>(A_DURATION) * p_out->shape.x /
                         node.val<int>(A_SUBITERATIONS));

    hmap::for_each_tile(
        {p_out, p_in, p_mask, &talus_map, p_deposition_map},
        [&node, &talus, iterations](std::vector<hmap::Array *> p_arrays,
                                    const hmap::TileRegion &)
        {
          hmap::Array *pa_out            = p_arrays[0];
          hmap::Array *pa_in             = p_arrays[1];
          hmap::Array *pa_mask           = p_arrays[2];
          hmap::Array *pa_talus          = p_arrays[3];
          hmap::Array *pa_deposition_map = p_arrays[4];

          *pa_out = *pa_in;

          hmap::gpu::sediment_deposition(*pa_out,
                                         pa_mask,
                                         *pa_talus,
                                         pa_deposition_map,
                                         node.val<float>(A_MAX_DEPOSITION),
                                         node.val<int>(A_SUBITERATIONS),
                                         iterations);
        },
        node.cfg().cm_cpu);

    p_out->smooth_overlap_buffers();

    if (p_deposition_map)
    {
      p_deposition_map->smooth_overlap_buffers();
      p_deposition_map->remap(0.f, 1.f, node.cfg().cm_cpu);
    }
  }
}

} // namespace hesiod
