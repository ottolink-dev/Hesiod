/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <fstream>

#include "highmap/filters.hpp"
#include "highmap/transform.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_ADVECTION_MASK = "advection_mask";
constexpr const char *P_ELEVATION      = "elevation";
constexpr const char *P_IN             = "input";
constexpr const char *P_MASK           = "mask";
constexpr const char *P_TEXTURE        = "texture";

constexpr const char *A_ADVECTION_LENGTH     = "advection_length";
constexpr const char *A_INERTIA              = "inertia";
constexpr const char *A_ITERATIONS           = "iterations";
constexpr const char *A_PARTICLE_DENSITY     = "particle_density";
constexpr const char *A_POST_FILTERING       = "post_filtering";
constexpr const char *A_POST_FILTERING_SIGMA = "post_filtering_sigma";
constexpr const char *A_REVERSE              = "reverse";
constexpr const char *A_SEED                 = "seed";
constexpr const char *A_VALUE_PERSISTENCE    = "value_persistence";

void setup_texture_advection_particle_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ELEVATION);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ADVECTION_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT, P_TEXTURE, CONFIG_TEX(node));

  // attribute(s)
  add_seed(node, A_SEED, "Seed");
  add_float(node, A_PARTICLE_DENSITY, "particle_density", 0.1f, 0.f, 1.f);
  add_int(node, A_ITERATIONS, "iterations", 5, 1, 100);
  add_float(node, A_ADVECTION_LENGTH, "advection_length", 0.1f, 0.f, 0.2f);
  add_float(node, A_VALUE_PERSISTENCE, "value_persistence", 0.99f, 0.8f, 1.f);
  add_float(node, A_INERTIA, "inertia", 0.f, 0.f, 1.f);
  add_bool(node, A_REVERSE, "reverse", false);
  add_bool(node, A_POST_FILTERING, "post_filtering", false);
  add_float(node, A_POST_FILTERING_SIGMA, "post_filtering_sigma", 0.07f, 0.f, 0.125f);

  setup_pre_process_mask_attributes(node);
}

void compute_texture_advection_particle_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray   *p_z   = node.get_value_ref<hmap::VirtualArray>(P_ELEVATION);
  hmap::VirtualTexture *p_tex = node.get_value_ref<hmap::VirtualTexture>(P_IN);

  if (p_z && p_tex)
  {
    hmap::VirtualArray *p_advection_mask = node.get_value_ref<hmap::VirtualArray>(
        P_ADVECTION_MASK);
    hmap::VirtualArray   *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);
    hmap::VirtualTexture *p_out  = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE);

    // prepare mask
    std::shared_ptr<hmap::VirtualArray> sp_mask = pre_process_mask(node, p_mask, *p_z);

    // number of particles based on the input particle density
    int nparticles = (int)(node.val<float>(A_PARTICLE_DENSITY) * p_out->shape.x *
                           p_out->shape.y);

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
                node.val<int>(A_ITERATIONS),
                nparticles,
                node.val<int>(A_SEED),
                node.val<bool>(A_REVERSE),
                node.val<bool>(A_POST_FILTERING),
                node.val<float>(A_POST_FILTERING_SIGMA),
                node.val<float>(A_ADVECTION_LENGTH),
                node.val<float>(A_VALUE_PERSISTENCE),
                node.val<float>(A_INERTIA),
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
