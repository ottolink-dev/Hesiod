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
constexpr const char *P_DR       = "dr";
constexpr const char *P_ENVELOPE = "envelope";
constexpr const char *P_OUT      = "output";

constexpr const char *A_ANGLE       = "angle";
constexpr const char *A_CENTER      = "center";
constexpr const char *A_KW          = "kw";
constexpr const char *A_PHASE_SHIFT = "phase_shift";
constexpr const char *A_SLANT_RATIO = "slant_ratio";

void setup_wave_triangular_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DR);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ENVELOPE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_KW, "kw", 2.f, 0.01f, FLT_MAX);
  add_float(node, A_ANGLE, "angle", 0.f, 0.f, 180.f, "{:.1f}°");
  add_float(node, A_SLANT_RATIO, "slant_ratio", 0.2f, 0.f, 1.f);
  add_float(node, A_PHASE_SHIFT, "phase_shift", 0.f, -180.f, 180.f, "{:.1f}°");
  add_xy(node, A_CENTER, "center");

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

void compute_wave_triangular_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // base noise function
  hmap::VirtualArray *p_dr  = node.get_value_ref<hmap::VirtualArray>(P_DR);
  hmap::VirtualArray *p_env = node.get_value_ref<hmap::VirtualArray>(P_ENVELOPE);
  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  const float phase_rad = node.val<float>(A_PHASE_SHIFT) * float(M_PI / 180.0);

  hmap::for_each_tile(
      {p_out, p_dr},
      [&](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
      {
        hmap::Array *pa_out = p_arrays[0];
        hmap::Array *pa_dr  = p_arrays[1];

        *pa_out = hmap::wave_triangular(region.shape,
                                        node.val<float>(A_KW),
                                        node.val<float>(A_ANGLE),
                                        node.val<float>(A_SLANT_RATIO),
                                        phase_rad,
                                        pa_dr,
                                        nullptr,
                                        node.val<glm::vec2>(A_CENTER),
                                        region.bbox);
      },
      node.cfg().cm_cpu);

  // post-process
  post_apply_enveloppe(node, *p_out, p_env);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
