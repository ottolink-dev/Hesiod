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
constexpr const char *P_IN    = "input";
constexpr const char *P_MASK  = "mask";
constexpr const char *P_NOISE = "noise";
constexpr const char *P_OUT   = "output";

constexpr const char *A_GAMMA = "gamma";
constexpr const char *A_VCUT  = "vcut";

void setup_recast_canyon_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_NOISE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_VCUT, "vcut", 0.5f, -1.f, 2.f);
  add_float(node, A_GAMMA, "gamma", 4.f, 0.01f, 10.f);
}

void compute_recast_canyon_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_noise = node.get_value_ref<hmap::VirtualArray>(P_NOISE);
    hmap::VirtualArray *p_mask  = node.get_value_ref<hmap::VirtualArray>(P_MASK);
    hmap::VirtualArray *p_out   = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    hmap::for_each_tile(
        {p_out, p_in, p_noise, p_mask},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          hmap::Array *pa_out   = p_arrays[0];
          hmap::Array *pa_in    = p_arrays[1];
          hmap::Array *pa_noise = p_arrays[2];
          hmap::Array *pa_mask  = p_arrays[3];

          *pa_out = *pa_in;

          hmap::recast_canyon(*pa_out,
                              node.val<float>(A_VCUT),
                              pa_mask,
                              node.val<float>(A_GAMMA),
                              pa_noise);
        },
        node.cfg().cm_cpu);
  }
}

} // namespace hesiod
