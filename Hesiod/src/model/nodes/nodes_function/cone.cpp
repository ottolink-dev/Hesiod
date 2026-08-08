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
constexpr const char *P_DX       = "dx";
constexpr const char *P_DY       = "dy";
constexpr const char *P_ENVELOPE = "envelope";
constexpr const char *P_OUT      = "output";

constexpr const char *A_APEX_ELEVATION = "apex_elevation";
constexpr const char *A_CENTER         = "center";
constexpr const char *A_POST_REMAP     = "post_remap";
constexpr const char *A_SLOPE          = "slope";
constexpr const char *A_SMOOTH_PROFILE = "smooth_profile";

void setup_cone_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ENVELOPE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_SLOPE, "slope", 4.f, 0.01f, FLT_MAX);
  add_float(node, A_APEX_ELEVATION, "apex_elevation", 1.f, 0.f, FLT_MAX);
  add_bool(node, A_SMOOTH_PROFILE, "smooth_profile", false);
  add_xy(node, A_CENTER, "center");

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

void compute_cone_node(BaseNode &node)
{

  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // base noise function
  hmap::VirtualArray *p_dx  = node.get_value_ref<hmap::VirtualArray>(P_DX);
  hmap::VirtualArray *p_dy  = node.get_value_ref<hmap::VirtualArray>(P_DY);
  hmap::VirtualArray *p_env = node.get_value_ref<hmap::VirtualArray>(P_ENVELOPE);
  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  hmap::for_each_tile(
      {p_out, p_dx, p_dy},
      [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
      {
        auto [pa_out, pa_dx, pa_dy] = unpack<3>(p_arrays);

        *pa_out = hmap::cone(region.shape,
                             node.val<float>(A_SLOPE),
                             node.val<float>(A_APEX_ELEVATION),
                             node.val<bool>(A_SMOOTH_PROFILE),
                             node.val<glm::vec2>(A_CENTER),
                             pa_dx,
                             pa_dy,
                             region.bbox);
      },
      node.cfg().cm_cpu);

  // post-process
  post_apply_enveloppe(node, *p_out, p_env);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
