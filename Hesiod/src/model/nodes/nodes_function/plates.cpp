/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/primitives.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_ENVELOPE = "envelope";
constexpr const char *P_OUT      = "output";

constexpr const char *A_BASE_NOISE_AMP = "base_noise_amp";
constexpr const char *A_DIRECTION      = "direction";
constexpr const char *A_KW             = "kw";
constexpr const char *A_KW_MULTIPLIER  = "kw_multiplier";
constexpr const char *A_MIX_RATIO      = "mix_ratio";
constexpr const char *A_OCTAVES        = "octaves";
constexpr const char *A_RUGOSITY       = "rugosity";
constexpr const char *A_SEED           = "seed";
constexpr const char *A_SLOPE          = "slope";

void setup_plates_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ENVELOPE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  glm::vec2 kw = {4.f, 4.f};
  add_wavenumber(node, A_KW, "Spatial Frequency", kw, 0.f, FLT_MAX, true);
  add_float(node, A_SLOPE, "Slope", 2.f, 0.f, FLT_MAX);
  add_int(node, A_DIRECTION, "Propagation Direction (D8)", 0, 0, 7);
  add_seed(node, A_SEED, "Seed");
  add_float(node, A_MIX_RATIO, "Mix", 0.9f, 0.f, 1.f);
  add_float(node, A_BASE_NOISE_AMP, "Amplitude", 0.05f, 0.f, 1.f);
  add_int(node, A_OCTAVES, "Octaves", 8, 0, 32);
  add_float(node, A_RUGOSITY, "Smoothness", 0.5f, 0.f, 1.f);
  add_float(node, A_KW_MULTIPLIER, "Frequency Scale", 2.f, 0.f, 16.f);

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

void compute_plates_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // base noise function
  hmap::VirtualArray *p_env = node.get_value_ref<hmap::VirtualArray>(P_ENVELOPE);
  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  float talus = node.val<float>(A_SLOPE) / float(p_out->shape.x);

  hmap::for_each_tile(
      {p_out},
      [&node, talus](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
      {
        auto [pa_out] = unpack<1>(p_arrays);

        *pa_out = hmap::gpu::plates(region.shape,
                                    node.val<glm::vec2>(A_KW),
                                    node.val<int>(A_SEED),
                                    talus,
                                    node.val<int>(A_DIRECTION),
                                    node.val<float>(A_MIX_RATIO),
                                    node.val<float>(A_BASE_NOISE_AMP),
                                    node.val<float>(A_KW_MULTIPLIER),
                                    node.val<int>(A_OCTAVES),
                                    node.val<float>(A_RUGOSITY),
                                    region.bbox);
      },
      node.cfg().cm_gpu);

  // post-process
  post_apply_enveloppe(node, *p_out, p_env);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
