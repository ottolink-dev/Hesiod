/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_IN   = "input";
constexpr const char *P_MASK = "mask";
constexpr const char *P_OUT  = "output";

constexpr const char *A_C_EROSION        = "c_erosion";
constexpr const char *A_CLIPPING_RATIO   = "clipping_ratio";
constexpr const char *A_PERSISTENCE      = "persistence";
constexpr const char *A_RADIUS           = "radius";
constexpr const char *A_TALUS_REF        = "talus_ref";
constexpr const char *A_UPSCALING_LEVELS = "upscaling_levels";

void setup_hydraulic_stream_upscale_amplification_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_C_EROSION, "c_erosion", 0.01f, 0.001f, 0.1f);
  add_float(node, A_TALUS_REF, "talus_ref", 0.1f, 0.01f, 10.f);
  add_float(node, A_RADIUS, "radius", 0.f, 0.f, 0.05f);
  add_float(node, A_CLIPPING_RATIO, "clipping_ratio", 10.f, 0.1f, 100.f);
  add_int(node, A_UPSCALING_LEVELS, "upscaling_levels", 1, 0, 4);
  add_float(node, A_PERSISTENCE, "Persistence", 0.5f, 0.f, 1.f);
}

void compute_hydraulic_stream_upscale_amplification_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);
    hmap::VirtualArray *p_out  = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    int ir = (int)(node.val<float>(A_RADIUS) * p_out->shape.x);

    hmap::for_each_tile(
        {p_out, p_in, p_mask},
        [&node, &ir](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in, pa_mask] = unpack<3>(p_arrays);
          *pa_out                       = *pa_in;

          hmap::hydraulic_stream_upscale_amplification(*pa_out,
                                                       pa_mask,
                                                       node.val<float>(A_C_EROSION),
                                                       node.val<float>(A_TALUS_REF),
                                                       node.val<int>(A_UPSCALING_LEVELS),
                                                       node.val<float>(A_PERSISTENCE),
                                                       ir,
                                                       node.val<float>(A_CLIPPING_RATIO));
        },
        node.cfg().cm_cpu);

    p_out->smooth_overlap_buffers();
  }
}

} // namespace hesiod
