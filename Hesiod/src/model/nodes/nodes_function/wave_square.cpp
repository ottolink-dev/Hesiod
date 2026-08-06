/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/primitives.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_wave_square_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dr");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "envelope");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("kw", "kw", 2.f, 0.01f, FLT_MAX);
  node.add_attr<FloatAttribute>("angle", "angle", 0.f, 0.f, 180.f, "{:.1f}°");
  node.add_attr<FloatAttribute>("phase_shift",
                                "phase_shift",
                                0.f,
                                -180.f,
                                180.f,
                                "{:.1f}°");
  node.add_attr<Vec2FloatAttribute>("center", "center");

  // attribute(s) order
  node.set_attr_ordered_key({"kw", "angle", "phase_shift", "center"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

void compute_wave_square_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // base noise function
  hmap::VirtualArray *p_dr = node.get_value_ref<hmap::VirtualArray>("dr");
  hmap::VirtualArray *p_env = node.get_value_ref<hmap::VirtualArray>("envelope");
  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

  const float phase_rad = node.get_attr<FloatAttribute>("phase_shift") *
                          float(M_PI / 180.0);

  hmap::for_each_tile(
      {p_out, p_dr},
      [&](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
      {
        hmap::Array *pa_out = p_arrays[0];
        hmap::Array *pa_dr = p_arrays[1];

        *pa_out = hmap::wave_square(region.shape,
                                    node.get_attr<FloatAttribute>("kw"),
                                    node.get_attr<FloatAttribute>("angle"),
                                    phase_rad,
                                    pa_dr,
                                    nullptr,
                                    nullptr,
                                    node.get_attr<Vec2FloatAttribute>("center"),
                                    region.bbox);
      },
      node.cfg().cm_cpu);

  // post-process
  post_apply_enveloppe(node, *p_out, p_env);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
