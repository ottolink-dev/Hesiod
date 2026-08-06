/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <fstream>

#include "highmap/filters.hpp"
#include "highmap/transform.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_texture_advection_particle_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "elevation");
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "advection_mask");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "mask");
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT, "texture", CONFIG_TEX(node));

  // attribute(s)
  node.add_attr<SeedAttribute>("seed", "Seed");
  node.add_attr<FloatAttribute>("particle_density", "particle_density", 0.1f, 0.f, 1.f);
  node.add_attr<IntAttribute>("iterations", "iterations", 5, 1, 100);
  node.add_attr<FloatAttribute>("advection_length", "advection_length", 0.1f, 0.f, 0.2f);
  node.add_attr<FloatAttribute>("value_persistence",
                                "value_persistence",
                                0.99f,
                                0.8f,
                                1.f);
  node.add_attr<FloatAttribute>("inertia", "inertia", 0.f, 0.f, 1.f);
  node.add_attr<BoolAttribute>("reverse", "reverse", false);
  node.add_attr<BoolAttribute>("post_filtering", "post_filtering", false);
  node.add_attr<FloatAttribute>("post_filtering_sigma",
                                "post_filtering_sigma",
                                0.07f,
                                0.f,
                                0.125f);

  // attribute(s) order
  node.set_attr_ordered_key({"_SEPARATOR_TEXT_Base Parameters",
                             "_TEXT_Initialization",
                             "seed",
                             "particle_density",
                             "iterations",
                             //
                             "_TEXT_Advection Control",
                             "advection_length",
                             "inertia",
                             "reverse",
                             //
                             "_TEXT_Value Persistence",
                             "value_persistence",
                             //
                             "_TEXT_Smoothing & Filtering",
                             "post_filtering",
                             "post_filtering_sigma"});

  setup_pre_process_mask_attributes(node);
}

void compute_texture_advection_particle_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray   *p_z = node.get_value_ref<hmap::VirtualArray>("elevation");
  hmap::VirtualTexture *p_tex = node.get_value_ref<hmap::VirtualTexture>("input");

  if (p_z && p_tex)
  {
    hmap::VirtualArray *p_advection_mask = node.get_value_ref<hmap::VirtualArray>(
        "advection_mask");
    hmap::VirtualArray   *p_mask = node.get_value_ref<hmap::VirtualArray>("mask");
    hmap::VirtualTexture *p_out = node.get_value_ref<hmap::VirtualTexture>("texture");

    // prepare mask
    std::shared_ptr<hmap::VirtualArray> sp_mask = pre_process_mask(node, p_mask, *p_z);

    // number of particles based on the input particle density
    int nparticles = (int)(node.get_attr<FloatAttribute>("particle_density") *
                           p_out->shape.x * p_out->shape.y);

    // apply advection separetely to each RGBA channels
    auto lambda = [&node, nparticles](hmap::VirtualArray *p_field_out,
                                      hmap::VirtualArray *p_z,
                                      hmap::VirtualArray *p_field,
                                      hmap::VirtualArray *p_advection_mask,
                                      hmap::VirtualArray *p_mask)
    {
      hmap::for_each_tile(
          {p_field_out, p_z, p_field, p_advection_mask, p_mask},
          [&node, nparticles](std::vector<hmap::Array *> p_arrays,
                              const hmap::TileRegion &)
          {
            auto [pa_field_out, pa_z, pa_field, pa_advection_mask, pa_mask] = unpack<5>(
                p_arrays);

            *pa_field_out = hmap::gpu::advection_particle(
                *pa_z,
                *pa_field,
                node.get_attr<IntAttribute>("iterations"),
                nparticles,
                node.get_attr<SeedAttribute>("seed"),
                node.get_attr<BoolAttribute>("reverse"),
                node.get_attr<BoolAttribute>("post_filtering"),
                node.get_attr<FloatAttribute>("post_filtering_sigma"),
                node.get_attr<FloatAttribute>("advection_length"),
                node.get_attr<FloatAttribute>("value_persistence"),
                node.get_attr<FloatAttribute>("inertia"),
                pa_advection_mask,
                pa_mask);
          },
          node.cfg().cm_gpu);
    };

    for (int nch = 0; nch < 4; nch++)
    {
      lambda(&(p_out->channel(nch)),
             p_z,
             &(p_tex->channel(nch)),
             p_advection_mask,
             p_mask);
      p_out->channel(nch).smooth_overlap_buffers();
    }
  }
}

} // namespace hesiod
