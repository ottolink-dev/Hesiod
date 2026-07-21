

/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/primitives.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_mountain_tibesti_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dx");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dy");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "envelope");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "out", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("elevation", "elevation", 0.5f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("scale", "scale", 1.f, 0.01f, FLT_MAX);
  node.add_attr<SeedAttribute>("seed", "Seed");
  node.add_attr<IntAttribute>("octaves", "Octaves", 8, 0, 32);
  node.add_attr<FloatAttribute>("peak_kw", "peak_kw", 22.f, 0.01f, FLT_MAX);
  node.add_attr<FloatAttribute>("rugosity", "rugosity", 0.f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("angle", "angle", 30.f, -180.f, 180.f, "{:.0f}°");
  node.add_attr<FloatAttribute>("angle_spread_ratio",
                                "angle_spread_ratio",
                                0.25f,
                                0.f,
                                1.f);
  node.add_attr<FloatAttribute>("gamma", "gamma", 1.5f, 0.01f, 4.f);
  node.add_attr<BoolAttribute>("add_deposition", "add_deposition", true);
  node.add_attr<FloatAttribute>("bulk_amp", "bulk_amp", 0.5f, 0.f, 2.f);
  node.add_attr<FloatAttribute>("base_noise_amp", "base_noise_amp", 0.05f, 0.f, 1.f);
  node.add_attr<Vec2FloatAttribute>("center", "center");

  // attribute(s) order
  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Main Parameters",
                             "_TEXT_Base Terrain Shape",
                             "elevation",
                             "scale",
                             "seed",
                             "octaves",
                             "peak_kw",
                             "bulk_amp",
                             "_TEXT_Position",
                             "center",
                             //
                             "_TEXT_Surface Detail & Structure",
                             "rugosity",
                             "angle",
                             "angle_spread_ratio",
                             "gamma",
                             "add_deposition",
                             //
                             "_TEXT_Noise & Modulation",
                             "base_noise_amp",
                             "_GROUPBOX_END_"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});

  // disable post-processing remap by default
  node.get_attr_ref<RangeAttribute>("post_remap")->set_is_active(false);
  node.get_attr_ref<RangeAttribute>("post_remap")->save_initial_state();
}

void compute_mountain_tibesti_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // base mountain_tibesti function
  hmap::VirtualArray *p_dx = node.get_value_ref<hmap::VirtualArray>("dx");
  hmap::VirtualArray *p_dy = node.get_value_ref<hmap::VirtualArray>("dy");
  hmap::VirtualArray *p_env = node.get_value_ref<hmap::VirtualArray>("envelope");
  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("out");

  hmap::for_each_tile(
      {p_out, p_dx, p_dy},
      [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
      {
        auto [pa_out, pa_dx, pa_dy] = unpack<3>(p_arrays);

        *pa_out = hmap::gpu::mountain_tibesti(
            region.shape,
            node.get_attr<SeedAttribute>("seed"),
            node.get_attr<FloatAttribute>("scale"),
            node.get_attr<IntAttribute>("octaves"),
            node.get_attr<FloatAttribute>("peak_kw"),
            node.get_attr<FloatAttribute>("rugosity"),
            node.get_attr<FloatAttribute>("angle"),
            node.get_attr<FloatAttribute>("angle_spread_ratio"),
            node.get_attr<FloatAttribute>("gamma"),
            node.get_attr<BoolAttribute>("add_deposition"),
            node.get_attr<FloatAttribute>("bulk_amp"),
            node.get_attr<FloatAttribute>("base_noise_amp"),
            node.get_attr<Vec2FloatAttribute>("center"),
            pa_dx,
            pa_dy,
            region.bbox);
      },
      node.cfg().cm_gpu);

  p_out->remap(0.f, node.get_attr<FloatAttribute>("elevation"), node.cfg().cm_cpu);

  // post-process
  post_apply_enveloppe(node, *p_out, p_env);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
