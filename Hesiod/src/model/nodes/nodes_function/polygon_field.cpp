/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/primitives.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_polygon_field_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dx");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dy");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dr");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "density");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "size");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "envelope");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<WaveNbAttribute>("kw", "Spatial Frequency");
  node.add_attr<SeedAttribute>("seed", "Seed");
  node.add_attr<FloatAttribute>("rmin", "rmin", 0.05f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("rmax", "rmax", 0.8f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("clamping_dist", "clamping_dist", 0.1f, 0.f, FLT_MAX);
  node.add_attr<FloatAttribute>("clamping_k", "clamping_k", 0.01f, 0.f, 0.2f);
  node.add_attr<FloatAttribute>("shift", "shift", 0.1f, 0.f, 1.f);
  node.add_attr<IntAttribute>("n_vertices_min", "n_vertices_min", 3, 3, 64);
  node.add_attr<IntAttribute>("n_vertices_max", "n_vertices_max", 8, 3, 64);
  node.add_attr<FloatAttribute>("density", "density", 0.1f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("jitter.x", "jitter.x", 1.f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("jitter.y", "jitter.y", 1.f, 0.f, 1.f);

  // attribute(s) order
  node.set_attr_ordered_key({"kw",
                             "seed",
                             "rmin",
                             "rmax",
                             "clamping_dist",
                             "clamping_k",
                             "shift",
                             "n_vertices_min",
                             "n_vertices_max",
                             "density",
                             "jitter.x",
                             "jitter.y"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

void compute_polygon_field_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // base noise function
  hmap::VirtualArray *p_dx = node.get_value_ref<hmap::VirtualArray>("dx");
  hmap::VirtualArray *p_dy = node.get_value_ref<hmap::VirtualArray>("dy");
  hmap::VirtualArray *p_dr = node.get_value_ref<hmap::VirtualArray>("dr");
  hmap::VirtualArray *p_density = node.get_value_ref<hmap::VirtualArray>("density");
  hmap::VirtualArray *p_size = node.get_value_ref<hmap::VirtualArray>("size");
  hmap::VirtualArray *p_env = node.get_value_ref<hmap::VirtualArray>("envelope");
  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

  hmap::for_each_tile(
      {p_out, p_dx, p_dy, p_dr, p_density, p_size},
      [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
      {
        hmap::Array *pa_out = p_arrays[0];
        hmap::Array *pa_dx = p_arrays[1];
        hmap::Array *pa_dy = p_arrays[2];
        hmap::Array *pa_dr = p_arrays[3];
        hmap::Array *pa_density = p_arrays[4];
        hmap::Array *pa_size = p_arrays[5];

        glm::vec2 jitter(node.get_attr<FloatAttribute>("jitter.x"),
                         node.get_attr<FloatAttribute>("jitter.y"));

        *pa_out = hmap::gpu::polygon_field(region.shape,
                                           node.get_attr<WaveNbAttribute>("kw"),
                                           node.get_attr<SeedAttribute>("seed"),
                                           node.get_attr<FloatAttribute>("rmin"),
                                           node.get_attr<FloatAttribute>("rmax"),
                                           node.get_attr<FloatAttribute>("clamping_dist"),
                                           node.get_attr<FloatAttribute>("clamping_k"),
                                           node.get_attr<IntAttribute>("n_vertices_min"),
                                           node.get_attr<IntAttribute>("n_vertices_max"),
                                           node.get_attr<FloatAttribute>("density"),
                                           jitter,
                                           node.get_attr<FloatAttribute>("shift"),
                                           pa_dx,
                                           pa_dy,
                                           pa_dr,
                                           pa_density,
                                           pa_size,
                                           region.bbox);
      },
      node.cfg().cm_gpu);

  // post-process
  post_apply_enveloppe(node, *p_out, p_env);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
