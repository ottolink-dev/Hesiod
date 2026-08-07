/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/opencl/gpu_opencl.hpp"
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
constexpr const char *P_DX       = "dx";
constexpr const char *P_DY       = "dy";
constexpr const char *P_ENVELOPE = "envelope";
constexpr const char *P_OUT      = "out";

constexpr const char *A_ANGLE       = "angle";
constexpr const char *A_ANGLE_SPAN  = "angle_span";
constexpr const char *A_DENSITY     = "density";
constexpr const char *A_EXP_SIGMA   = "exp_sigma";
constexpr const char *A_K_SMOOTHING = "k_smoothing";
constexpr const char *A_RETURN_TYPE = "return_type";
constexpr const char *A_SEED        = "seed";
constexpr const char *A_SQRT_OUTPUT = "sqrt_output";

void setup_vorolines_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ENVELOPE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_enum(node,
           A_RETURN_TYPE,
           "return_type",
           enum_mappings.voronoi_return_type_map,
           "F1: squared distance to the closest point");
  add_float(node, A_DENSITY, "density", 8.f, 0.f, 100.f);
  add_seed(node, A_SEED, "Seed");
  add_float(node, A_K_SMOOTHING, "k_smoothing", 0.f, 0.f, 1.f);
  add_float(node, A_EXP_SIGMA, "exp_sigma", 0.1f, 0.f, 0.1f);
  add_float(node, A_ANGLE, "angle", 0.f, 0.f, 180.f);
  add_float(node, A_ANGLE_SPAN, "angle_span", 90.f, 0.f, 180.f);
  add_bool(node, A_SQRT_OUTPUT, "sqrt_output", false);

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_vorolines_node(BaseNode &node)
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

        hmap::VoronoiReturnType rtype = (hmap::VoronoiReturnType)node.val<int>(
            A_RETURN_TYPE);

        *pa_out = hmap::gpu::vorolines(region.shape,
                                       node.val<float>(A_DENSITY),
                                       node.val<int>(A_SEED),
                                       node.val<float>(A_K_SMOOTHING),
                                       node.val<float>(A_EXP_SIGMA),
                                       M_PI / 180.f * node.val<float>(A_ANGLE),
                                       M_PI / 180.f * node.val<float>(A_ANGLE_SPAN),
                                       rtype,
                                       pa_dx,
                                       pa_dy,
                                       region.bbox);
      },
      node.cfg().cm_gpu);

  // apply square root
  p_out->remap(0.f, 1.f, node.cfg().cm_cpu);

  if (node.val<bool>(A_SQRT_OUTPUT))
    hmap::for_each_tile(
        {p_out},
        [](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          hmap::Array *pa_out = p_arrays[0];
          *pa_out             = hmap::sqrt(*pa_out);
        },
        node.cfg().cm_cpu);

  // post-process
  post_apply_enveloppe(node, *p_out, p_env);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
