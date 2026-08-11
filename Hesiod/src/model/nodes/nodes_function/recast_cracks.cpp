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
constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "output";

constexpr const char *A_CUT_MAX     = "cut_max";
constexpr const char *A_CUT_MIN     = "cut_min";
constexpr const char *A_K_SMOOTHING = "k_smoothing";

void setup_recast_cracks_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_CUT_MIN, "cut_min", 0.05f, 0.f, 1.f);
  add_float(node, A_CUT_MAX, "cut_max", 0.5f, 0.f, 1.f);
  add_float(node, A_K_SMOOTHING, "k_smoothing", 0.01f, 0.f, 1.f);
}

void compute_recast_cracks_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    float hmin = p_in->min(node.cfg().cm_cpu);
    float hmax = p_in->max(node.cfg().cm_cpu);

    hmap::for_each_tile(
        {p_out, p_in},
        [&node, hmin, hmax](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out              = *pa_in;

          hmap::recast_cracks(*pa_out,
                              node.val<float>(A_CUT_MIN),
                              node.val<float>(A_CUT_MAX),
                              node.val<float>(A_K_SMOOTHING),
                              hmin,
                              hmax);
        },
        node.cfg().cm_gpu);

    p_out->smooth_overlap_buffers();
  }
}

} // namespace hesiod
