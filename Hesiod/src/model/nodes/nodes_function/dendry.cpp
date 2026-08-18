/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
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
constexpr const char *P_CONTROL  = "control";
constexpr const char *P_DX       = "dx";
constexpr const char *P_DY       = "dy";
constexpr const char *P_ENVELOPE = "envelope";
constexpr const char *P_OUT      = "out";

constexpr const char *A_DISPLACEMENT                = "displacement";
constexpr const char *A_EPS                         = "eps";
constexpr const char *A_KW                          = "kw";
constexpr const char *A_NOISE_AMPLITUDE_PROPORTION  = "noise_amplitude_proportion";
constexpr const char *A_PRIMITIVES_RESOLUTION_STEPS = "primitives_resolution_steps";
constexpr const char *A_RESOLUTION                  = "resolution";
constexpr const char *A_SEED                        = "seed";
constexpr const char *A_SLOPE_POWER                 = "slope_power";
constexpr const char *A_SUBSAMPLING                 = "subsampling";

void setup_dendry_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_CONTROL);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ENVELOPE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_int(node, A_SUBSAMPLING, "subsampling", 4, 1, 8);

  add_wavenumber(node, A_KW, "Spatial Frequency", glm::vec2(8.f, 8.f), 0.f, FLT_MAX);
  add_seed(node, A_SEED, "Seed");
  add_float(node, A_EPS, "eps", 0.2f, 0.f, 1.f);
  add_int(node, A_RESOLUTION, "resolution", 1, 1, 8);
  add_float(node, A_DISPLACEMENT, "displacement", 0.075f, 0.f, 0.2f);
  add_int(node, A_PRIMITIVES_RESOLUTION_STEPS, "primitives_resolution_steps", 3, 1, 8);
  add_float(node, A_SLOPE_POWER, "slope_power", 1.f, 0.f, 2.f);
  add_float(node,
            A_NOISE_AMPLITUDE_PROPORTION,
            "noise_amplitude_proportion",
            0.01f,
            0.f,
            1.f);

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_dendry_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // base noise function
  hmap::VirtualArray *p_dx   = node.get_value_ref<hmap::VirtualArray>(P_DX);
  hmap::VirtualArray *p_dy   = node.get_value_ref<hmap::VirtualArray>(P_DY);
  hmap::VirtualArray *p_ctrl = node.get_value_ref<hmap::VirtualArray>(P_CONTROL);
  hmap::VirtualArray *p_env  = node.get_value_ref<hmap::VirtualArray>(P_ENVELOPE);
  hmap::VirtualArray *p_out  = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (p_ctrl)
  {
    // TODO shape
    hmap::Array ctrl_array = p_ctrl->to_array(glm::ivec2(128, 128), node.cfg().cm_cpu);

    hmap::for_each_tile(
        {p_out, p_dx, p_dy},
        [&node, &ctrl_array](std::vector<hmap::Array *> p_arrays,
                             const hmap::TileRegion    &region)
        {
          hmap::Array *pa_out = p_arrays[0];
          hmap::Array *pa_dx  = p_arrays[1];
          hmap::Array *pa_dy  = p_arrays[2];

          *pa_out = hmap::dendry(region.shape,
                                 node.val<glm::vec2>(A_KW),
                                 node.val<int>(A_SEED),
                                 ctrl_array,
                                 node.val<float>(A_EPS),
                                 node.val<int>(A_RESOLUTION),
                                 node.val<float>(A_DISPLACEMENT),
                                 node.val<int>(A_PRIMITIVES_RESOLUTION_STEPS),
                                 node.val<float>(A_SLOPE_POWER),
                                 node.val<float>(A_NOISE_AMPLITUDE_PROPORTION),
                                 true, // add noise
                                 0.5f, // overlap
                                 pa_dx,
                                 pa_dy,
                                 region.bbox,
                                 node.val<int>(A_SUBSAMPLING));
        },
        node.cfg().cm_cpu);

    p_out->smooth_overlap_buffers();

    // post-process
    post_apply_enveloppe(node, *p_out, p_env);
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
