/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/opencl/gpu_opencl.hpp"
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
constexpr const char *P_DENSITY  = "density";
constexpr const char *P_DR       = "dr";
constexpr const char *P_DX       = "dx";
constexpr const char *P_DY       = "dy";
constexpr const char *P_ENVELOPE = "envelope";
constexpr const char *P_OUT      = "output";
constexpr const char *P_SIZE     = "size";

constexpr const char *A_CLAMPING_DIST  = "clamping_dist";
constexpr const char *A_CLAMPING_K     = "clamping_k";
constexpr const char *A_DENSITY        = "density";
constexpr const char *A_JITTER_X       = "jitter.x";
constexpr const char *A_JITTER_Y       = "jitter.y";
constexpr const char *A_KW             = "kw";
constexpr const char *A_LACUNARITY     = "lacunarity";
constexpr const char *A_N_VERTICES_MAX = "n_vertices_max";
constexpr const char *A_N_VERTICES_MIN = "n_vertices_min";
constexpr const char *A_OCTAVES        = "octaves";
constexpr const char *A_PERSISTENCE    = "persistence";
constexpr const char *A_RMAX           = "rmax";
constexpr const char *A_RMIN           = "rmin";
constexpr const char *A_SEED           = "seed";
constexpr const char *A_SHIFT          = "shift";

void setup_polygon_field_fbm_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DR);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DENSITY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_SIZE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ENVELOPE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_wavenumber(node, A_KW, "Spatial Frequency");
  add_seed(node, A_SEED, "Seed");
  add_float(node, A_RMIN, "rmin", 0.05f, 0.f, 1.f);
  add_float(node, A_RMAX, "rmax", 0.8f, 0.f, 1.f);
  add_float(node, A_CLAMPING_DIST, "clamping_dist", 0.1f, 0.f, FLT_MAX);
  add_float(node, A_CLAMPING_K, "clamping_k", 0.01f, 0.f, 0.2f);
  add_float(node, A_SHIFT, "shift", 0.1f, 0.f, 1.f);
  add_int(node, A_N_VERTICES_MIN, "n_vertices_min", 3, 3, 64);
  add_int(node, A_N_VERTICES_MAX, "n_vertices_max", 8, 3, 64);
  add_float(node, A_DENSITY, "density", 0.1f, 0.f, 1.f);
  add_float(node, A_JITTER_X, "jitter.x", 1.f, 0.f, 1.f);
  add_float(node, A_JITTER_Y, "jitter.y", 1.f, 0.f, 1.f);
  add_int(node, A_OCTAVES, "Octaves", 8, 0, 32);
  add_float(node, A_PERSISTENCE, "Persistence", 0.5f, 0.f, 1.f);
  add_float(node, A_LACUNARITY, "Lacunarity", 2.f, 0.01f, 4.f);

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

void compute_polygon_field_fbm_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // base noise function
  hmap::VirtualArray *p_dx      = node.get_value_ref<hmap::VirtualArray>(P_DX);
  hmap::VirtualArray *p_dy      = node.get_value_ref<hmap::VirtualArray>(P_DY);
  hmap::VirtualArray *p_dr      = node.get_value_ref<hmap::VirtualArray>(P_DR);
  hmap::VirtualArray *p_density = node.get_value_ref<hmap::VirtualArray>(P_DENSITY);
  hmap::VirtualArray *p_size    = node.get_value_ref<hmap::VirtualArray>(P_SIZE);
  hmap::VirtualArray *p_env     = node.get_value_ref<hmap::VirtualArray>(P_ENVELOPE);
  hmap::VirtualArray *p_out     = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  hmap::for_each_tile(
      {p_out, p_dx, p_dy, p_dr, p_density, p_size},
      [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
      {
        hmap::Array *pa_out     = p_arrays[0];
        hmap::Array *pa_dx      = p_arrays[1];
        hmap::Array *pa_dy      = p_arrays[2];
        hmap::Array *pa_dr      = p_arrays[3];
        hmap::Array *pa_density = p_arrays[4];
        hmap::Array *pa_size    = p_arrays[5];

        glm::vec2 jitter(node.val<float>(A_JITTER_X), node.val<float>(A_JITTER_Y));

        *pa_out = hmap::gpu::polygon_field_fbm(region.shape,
                                               node.val<glm::vec2>(A_KW),
                                               node.val<int>(A_SEED),
                                               node.val<float>(A_RMIN),
                                               node.val<float>(A_RMAX),
                                               node.val<float>(A_CLAMPING_DIST),
                                               node.val<float>(A_CLAMPING_K),
                                               node.val<int>(A_N_VERTICES_MIN),
                                               node.val<int>(A_N_VERTICES_MAX),
                                               node.val<float>(A_DENSITY),
                                               jitter,
                                               node.val<float>(A_SHIFT),
                                               node.val<int>(A_OCTAVES),
                                               node.val<float>(A_PERSISTENCE),
                                               node.val<float>(A_LACUNARITY),
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
