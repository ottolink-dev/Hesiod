/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/primitives.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_cone_complex_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dx");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dy");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "control");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "envelope");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("radius", "radius", 0.5f, 0.01f, FLT_MAX);
  node.add_attr<FloatAttribute>("alpha", "alpha", 1.2f, 0.01f, 4.f);
  node.add_attr<BoolAttribute>("smooth_profile", "smooth_profile", true);
  node.add_attr<FloatAttribute>("valley_amp", "valley_amp", 0.2f, 0.f, 2.f);
  node.add_attr<IntAttribute>("valley_nb", "valley_nb", 5, 0, 32);
  node.add_attr<FloatAttribute>("valley_decay_ratio",
                                "valley_decay_ratio",
                                0.5f,
                                0.f,
                                1.f);
  node.add_attr<FloatAttribute>("valley_angle0",
                                "valley_angle0",
                                0.f,
                                -180.f,
                                180.f,
                                "{:.1f}°");
  node.add_attr<EnumAttribute>("erosion_profile",
                               "erosion_profile",
                               enum_mappings.erosion_profile_map,
                               "Triangle Grenier");
  node.add_attr<FloatAttribute>("erosion_delta", "erosion_delta", 0.01f, 0.f, 0.2f);
  node.add_attr<FloatAttribute>("radial_waviness_amp",
                                "radial_waviness_amp",
                                0.05f,
                                0.f,
                                0.5f);
  node.add_attr<FloatAttribute>("radial_waviness_kw",
                                "radial_waviness_kw",
                                2.f,
                                0.f,
                                16.f);
  node.add_attr<FloatAttribute>("bias_angle",
                                "bias_angle",
                                30.f,
                                -180.f,
                                180.f,
                                "{:.1f}°");
  node.add_attr<FloatAttribute>("bias_amp", "bias_amp", 0.1f, 0.f, 2.f);
  node.add_attr<FloatAttribute>("bias_exponent", "bias_exponent", 1.f, 0.01f, 4.f);
  node.add_attr<Vec2FloatAttribute>("center", "center");

  // attribute(s) order
  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Main Parameters",
                             //
                             "_TEXT_Shape & Size",
                             "radius",
                             "alpha",
                             "smooth_profile",
                             //
                             "_TEXT_Position",
                             "center",
                             //
                             "_TEXT_Erosion valleys",
                             "valley_amp",
                             "valley_nb",
                             "valley_decay_ratio",
                             "valley_angle0",
                             "erosion_profile",
                             "erosion_delta",
                             "radial_waviness_amp",
                             "radial_waviness_kw",
                             //
                             "_TEXT_Directional bias",
                             "bias_angle",
                             "bias_amp",
                             "bias_exponent",
                             //
                             "_GROUPBOX_END_"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

void compute_cone_complex_node(BaseNode &node)
{

  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // base noise function
  hmap::VirtualArray *p_dx = node.get_value_ref<hmap::VirtualArray>("dx");
  hmap::VirtualArray *p_dy = node.get_value_ref<hmap::VirtualArray>("dy");
  hmap::VirtualArray *p_ctrl = node.get_value_ref<hmap::VirtualArray>("control");
  hmap::VirtualArray *p_env = node.get_value_ref<hmap::VirtualArray>("envelope");
  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

  hmap::for_each_tile(
      {p_out, p_dx, p_dy, p_ctrl},
      [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
      {
        auto [pa_out, pa_dx, pa_dy, pa_ctrl] = unpack<4>(p_arrays);

        *pa_out = hmap::cone_complex(
            region.shape,
            node.get_attr<FloatAttribute>("alpha"),
            node.get_attr<FloatAttribute>("radius"),
            node.get_attr<BoolAttribute>("smooth_profile"),
            node.get_attr<FloatAttribute>("valley_amp"),
            node.get_attr<IntAttribute>("valley_nb"),
            node.get_attr<FloatAttribute>("valley_decay_ratio"),
            node.get_attr<FloatAttribute>("valley_angle0"),
            (hmap::ErosionProfile)node.get_attr<EnumAttribute>("erosion_profile"),
            node.get_attr<FloatAttribute>("erosion_delta"),
            node.get_attr<FloatAttribute>("radial_waviness_amp"),
            node.get_attr<FloatAttribute>("radial_waviness_kw"),
            node.get_attr<FloatAttribute>("bias_angle"),
            node.get_attr<FloatAttribute>("bias_amp"),
            node.get_attr<FloatAttribute>("bias_exponent"),
            node.get_attr<Vec2FloatAttribute>("center"),
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
