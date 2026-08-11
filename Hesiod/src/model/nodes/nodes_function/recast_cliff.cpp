/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_IN   = "input";
constexpr const char *P_MASK = "mask";
constexpr const char *P_OUT  = "output";

constexpr const char *A_AMPLITUDE    = "amplitude";
constexpr const char *A_GAIN         = "gain";
constexpr const char *A_RADIUS       = "radius";
constexpr const char *A_TALUS_GLOBAL = "talus_global";

void setup_recast_cliff_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_TALUS_GLOBAL, "talus_global", 1.f, 0.f, 5.f);
  add_float(node, A_RADIUS, "radius", 0.1f, 0.01f, 0.5f);
  add_float(node, A_AMPLITUDE, "amplitude", 0.1f, 0.f, 1.f);
  add_float(node, A_GAIN, "gain", 2.f, 0.01f, 10.f);
}

void compute_recast_cliff_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);
    hmap::VirtualArray *p_out  = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    float talus = node.val<float>(A_TALUS_GLOBAL) / (float)p_out->shape.x;
    int   ir    = std::max(1, (int)(node.val<float>(A_RADIUS) * p_out->shape.x));

    hmap::for_each_tile(
        {p_out, p_in, p_mask},
        [&node, talus, ir](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in, pa_mask] = unpack<3>(p_arrays);
          *pa_out                       = *pa_in;

          hmap::recast_cliff(*pa_out,
                             talus,
                             ir,
                             node.val<float>(A_AMPLITUDE),
                             pa_mask,
                             node.val<float>(A_GAIN));
        },
        node.cfg().cm_cpu);

    p_out->smooth_overlap_buffers();
  }
}

} // namespace hesiod
