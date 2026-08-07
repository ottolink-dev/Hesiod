/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
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
constexpr const char *P_CONTROL  = "control";
constexpr const char *P_DX       = "dx";
constexpr const char *P_DY       = "dy";
constexpr const char *P_ENVELOPE = "envelope";
constexpr const char *P_OUT      = "output";

constexpr const char *A_ALPHA               = "alpha";
constexpr const char *A_BIAS_AMP            = "bias_amp";
constexpr const char *A_BIAS_ANGLE          = "bias_angle";
constexpr const char *A_BIAS_EXPONENT       = "bias_exponent";
constexpr const char *A_CENTER              = "center";
constexpr const char *A_EROSION_DELTA       = "erosion_delta";
constexpr const char *A_EROSION_PROFILE     = "erosion_profile";
constexpr const char *A_RADIAL_WAVINESS_AMP = "radial_waviness_amp";
constexpr const char *A_RADIAL_WAVINESS_KW  = "radial_waviness_kw";
constexpr const char *A_RADIUS              = "radius";
constexpr const char *A_SMOOTH_PROFILE      = "smooth_profile";
constexpr const char *A_VALLEY_AMP          = "valley_amp";
constexpr const char *A_VALLEY_ANGLE0       = "valley_angle0";
constexpr const char *A_VALLEY_DECAY_RATIO  = "valley_decay_ratio";
constexpr const char *A_VALLEY_NB           = "valley_nb";

void setup_cone_complex_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_CONTROL);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ENVELOPE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_RADIUS, "radius", 0.5f, 0.01f, FLT_MAX);
  add_float(node, A_ALPHA, "alpha", 1.2f, 0.01f, 4.f);
  add_bool(node, A_SMOOTH_PROFILE, "smooth_profile", true);
  add_float(node, A_VALLEY_AMP, "valley_amp", 0.2f, 0.f, 2.f);
  add_int(node, A_VALLEY_NB, "valley_nb", 5, 0, 32);
  add_float(node, A_VALLEY_DECAY_RATIO, "valley_decay_ratio", 0.5f, 0.f, 1.f);
  add_float(node, A_VALLEY_ANGLE0, "valley_angle0", 0.f, -180.f, 180.f, "{:.1f}°");
  add_enum(node,
           A_EROSION_PROFILE,
           "erosion_profile",
           enum_mappings.erosion_profile_map,
           "Triangle Grenier");
  add_float(node, A_EROSION_DELTA, "erosion_delta", 0.01f, 0.f, 0.2f);
  add_float(node, A_RADIAL_WAVINESS_AMP, "radial_waviness_amp", 0.05f, 0.f, 0.5f);
  add_float(node, A_RADIAL_WAVINESS_KW, "radial_waviness_kw", 2.f, 0.f, 16.f);
  add_float(node, A_BIAS_ANGLE, "bias_angle", 30.f, -180.f, 180.f, "{:.1f}°");
  add_float(node, A_BIAS_AMP, "bias_amp", 0.1f, 0.f, 2.f);
  add_float(node, A_BIAS_EXPONENT, "bias_exponent", 1.f, 0.01f, 4.f);
  add_xy(node, A_CENTER, "center");

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

void compute_cone_complex_node(BaseNode &node)
{

  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // base noise function
  hmap::VirtualArray *p_dx   = node.get_value_ref<hmap::VirtualArray>(P_DX);
  hmap::VirtualArray *p_dy   = node.get_value_ref<hmap::VirtualArray>(P_DY);
  hmap::VirtualArray *p_ctrl = node.get_value_ref<hmap::VirtualArray>(P_CONTROL);
  hmap::VirtualArray *p_env  = node.get_value_ref<hmap::VirtualArray>(P_ENVELOPE);
  hmap::VirtualArray *p_out  = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  hmap::for_each_tile(
      {p_out, p_dx, p_dy, p_ctrl},
      [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
      {
        auto [pa_out, pa_dx, pa_dy, pa_ctrl] = unpack<4>(p_arrays);

        *pa_out = hmap::cone_complex(
            region.shape,
            node.val<float>(A_ALPHA),
            node.val<float>(A_RADIUS),
            node.val<bool>(A_SMOOTH_PROFILE),
            node.val<float>(A_VALLEY_AMP),
            node.val<int>(A_VALLEY_NB),
            node.val<float>(A_VALLEY_DECAY_RATIO),
            node.val<float>(A_VALLEY_ANGLE0),
            (hmap::ErosionProfile)node.val<int>(A_EROSION_PROFILE),
            node.val<float>(A_EROSION_DELTA),
            node.val<float>(A_RADIAL_WAVINESS_AMP),
            node.val<float>(A_RADIAL_WAVINESS_KW),
            node.val<float>(A_BIAS_ANGLE),
            node.val<float>(A_BIAS_AMP),
            node.val<float>(A_BIAS_EXPONENT),
            node.val<glm::vec2>(A_CENTER),
            pa_ctrl,
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
