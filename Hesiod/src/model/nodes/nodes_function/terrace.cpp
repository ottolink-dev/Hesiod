/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/range.hpp"

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

constexpr const char *A_GAIN        = "gain";
constexpr const char *A_NLEVELS     = "nlevels";
constexpr const char *A_NOISE_RATIO = "noise_ratio";
constexpr const char *A_SEED        = "seed";

void setup_terrace_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_NOISE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_int(node, A_NLEVELS, "nlevels", 4, 1, 32);
  add_float(node, A_GAIN, "gain", 0.8f, 0.f, 1.f);
  add_float(node, A_NOISE_RATIO, "noise_ratio", 0.1f, 0.f, 0.5f);
  add_seed(node, A_SEED, "Seed");

  setup_pre_process_mask_attributes(node);
}

void compute_terrace_node(BaseNode &node)
{
  Logger::log()->error("Terrace node is deprecated, use StrataTerrace node");

  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_noise = node.get_value_ref<hmap::VirtualArray>(P_NOISE);
    hmap::VirtualArray *p_mask  = node.get_value_ref<hmap::VirtualArray>(P_MASK);
    hmap::VirtualArray *p_out   = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    // prepare mask
    std::shared_ptr<hmap::VirtualArray> sp_mask = pre_process_mask(node, p_mask, *p_in);

    float hmin = p_in->min(node.cfg().cm_cpu);
    float hmax = p_in->max(node.cfg().cm_cpu);

    hmap::for_each_tile(
        {p_out, p_in, p_noise, p_mask},
        [&node, hmin, hmax](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          hmap::Array *pa_out   = p_arrays[0];
          hmap::Array *pa_in    = p_arrays[1];
          hmap::Array *pa_noise = p_arrays[2];
          hmap::Array *pa_mask  = p_arrays[3];

          *pa_out = *pa_in;

          hmap::terrace(*pa_out,
                        node.val<int>(A_SEED),
                        node.val<int>(A_NLEVELS),
                        pa_mask,
                        node.val<float>(A_GAIN),
                        node.val<float>(A_NOISE_RATIO),
                        pa_noise,
                        hmin,
                        hmax);
        },
        node.cfg().cm_gpu);
  }
}

} // namespace hesiod
