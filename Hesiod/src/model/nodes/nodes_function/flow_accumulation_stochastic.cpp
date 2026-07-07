/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/hydrology/hydrology.hpp"
#include "highmap/math.hpp"
#include "highmap/opencl/gpu_opencl.hpp"

#include "attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

constexpr const char *P_INPUT = "input";
constexpr const char *P_SOURCE = "source";
constexpr const char *P_DECAY = "decay";
constexpr const char *P_FLUX = "flux";

constexpr const char *A_N_SAMPLES = "n_samples";
constexpr const char *A_SEED = "seed";
constexpr const char *A_LOG_SCALE = "log_scale";

void setup_flow_accumulation_stochastic_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_INPUT);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_SOURCE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DECAY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_FLUX, CONFIG(node));

  // attribute(s)
  // clang-format off
  node.add_attr<IntAttribute>(A_N_SAMPLES, "Samples", 1 << 19, 1024, 1 << 22);
  node.add_attr<SeedAttribute>(A_SEED, "Seed");
  node.add_attr<BoolAttribute>(A_LOG_SCALE, "Log Scale", true);
  // clang-format on

  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Accumulation",
                             A_N_SAMPLES,
                             A_SEED,
                             A_LOG_SCALE,
                             "_GROUPBOX_END_"});

  setup_post_process_heightmap_attributes(node, {.add_mix = false});
}

void compute_flow_accumulation_stochastic_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto *p_in = node.get_value_ref<hmap::VirtualArray>(P_INPUT);
  auto *p_source = node.get_value_ref<hmap::VirtualArray>(P_SOURCE);
  auto *p_decay = node.get_value_ref<hmap::VirtualArray>(P_DECAY);
  auto *p_flux = node.get_value_ref<hmap::VirtualArray>(P_FLUX);

  if (!p_in)
    return;

  const int  n_samples = node.get_attr<IntAttribute>(A_N_SAMPLES);
  const uint seed = node.get_attr<SeedAttribute>(A_SEED);
  const bool log_scale = node.get_attr<BoolAttribute>(A_LOG_SCALE);

  hmap::for_each_tile(
      {p_in, p_source, p_decay},
      {p_flux},
      [&](std::vector<const hmap::Array *> in,
          std::vector<hmap::Array *>       out,
          const hmap::TileRegion &)
      {
        auto [pa_in, pa_source, pa_decay] = unpack<3>(in);
        hmap::Array *pa_flux = out[0];

        *pa_flux = hmap::gpu::flow_accumulation_stochastic(*pa_in,
                                                           n_samples,
                                                           seed,
                                                           pa_source,
                                                           pa_decay);

        if (log_scale)
          *pa_flux = hmap::log10(*pa_flux + 1.f);
      },
      node.cfg().cm_gpu);

  p_flux->smooth_overlap_buffers();
  post_process_heightmap(node, *p_flux, nullptr);
}

} // namespace hesiod
