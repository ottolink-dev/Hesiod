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

constexpr const char *A_A         = "a";
constexpr const char *A_ANGLE     = "angle";
constexpr const char *A_B         = "b";
constexpr const char *A_CENTER    = "center";
constexpr const char *A_REVERSE_X = "reverse_x";
constexpr const char *A_REVERSE_Y = "reverse_y";
constexpr const char *A_V0        = "v0";

void setup_paraboloid_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ENVELOPE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_ANGLE, "angle", 0.f, -180.f, 180.f);
  add_float(node, A_A, "a", 1.f, 0.01f, 5.f);
  add_float(node, A_B, "b", 1.f, 0.01f, 5.f);
  add_float(node, A_V0, "v0", 0.f, -2.f, 2.f);
  add_bool(node, A_REVERSE_X, "reverse_x", false);
  add_bool(node, A_REVERSE_Y, "reverse_y", false);
  add_xy(node, A_CENTER, "center");
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_paraboloid_node(BaseNode &node)
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

        *pa_out = hmap::paraboloid(region.shape,
                                   node.val<float>(A_ANGLE),
                                   node.val<float>(A_A),
                                   node.val<float>(A_B),
                                   node.val<float>(A_V0),
                                   node.val<bool>(A_REVERSE_X),
                                   node.val<bool>(A_REVERSE_Y),
                                   pa_dx,
                                   pa_dy,
                                   node.val<glm::vec2>(A_CENTER),
                                   region.bbox);
      },
      node.cfg().cm_cpu);

  // post-process
  post_apply_enveloppe(node, *p_out, p_env);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
