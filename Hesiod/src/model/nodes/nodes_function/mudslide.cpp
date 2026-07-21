/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"
#include "highmap/opencl/gpu_opencl.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"
#include "hesiod/model/utils.hpp"

using namespace attr;

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN = "input";
constexpr const char *P_LANDSLIDE_MASK = "landslide_mask";
constexpr const char *P_OUT = "output";
constexpr const char *P_DEPTH = "depth";

constexpr const char *A_DEPTH = "depth";
constexpr const char *A_SLOPE_LIMIT = "slope_limit";
constexpr const char *A_DURATION = "duration";
constexpr const char *A_SOLVER_STRIDE = "solver_stride";
constexpr const char *A_DEPTH_MAP_EXPONENT = "depth_map_exponent";
constexpr const char *A_VISCOSITY_LAW_POWER = "viscosity_law_power";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_mudslide_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_LANDSLIDE_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_DEPTH, CONFIG(node));

  // attribute(s)
  // clang-format off
  node.add_attr<FloatAttribute>(A_DEPTH, "depth", 0.2f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_SLOPE_LIMIT, "slope_limit", 2.f, 0.f, FLT_MAX);
  node.add_attr<FloatAttribute>(A_DURATION, "Simulation Duration", 1.f, 0.f, FLT_MAX);
  node.add_attr<IntAttribute>(A_SOLVER_STRIDE, "Solver Iteration Stride", 8, 1, 32);
  node.add_attr<FloatAttribute>(A_DEPTH_MAP_EXPONENT, "depth_map_exponent", 0.5f, 0.1f, 2.f);
  node.add_attr<FloatAttribute>(A_VISCOSITY_LAW_POWER, "viscosity_law_power", 2.f, 1.f, 4.f);
  // clang-format on

  // attribute(s) order
  node.set_attr_ordered_key({A_DEPTH,
                             A_SLOPE_LIMIT,
                             A_DURATION,
                             A_SOLVER_STRIDE,
                             A_DEPTH_MAP_EXPONENT,
                             A_VISCOSITY_LAW_POWER});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_mudslide_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_map = node.get_value_ref<hmap::VirtualArray>(P_LANDSLIDE_MASK);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);
  auto *p_depth = node.get_value_ref<hmap::VirtualArray>(P_DEPTH);

  if (!p_in)
    return;

  // --- Parameters wrapper

  const auto params = [&node, p_out]()
  {
    struct P
    {
      float depth;
      float depth_map_exponent;
      float talus_limit;
      float viscosity_law_power;
      int   nx_strided;
      int   iterations;
      int   solver_stride;
    };

    const int   nx = p_out->shape.x;
    const int   stride = node.get_attr<IntAttribute>(A_SOLVER_STRIDE);
    const float duration = node.get_attr<FloatAttribute>(A_DURATION);
    const int   nx_strided = int(float(nx) / stride);
    const int   iterations = int(duration * nx_strided);
    const float talus_limit = node.get_attr<FloatAttribute>(A_SLOPE_LIMIT) / nx_strided;

    return P{.depth = node.get_attr<FloatAttribute>(A_DEPTH),
             .depth_map_exponent = node.get_attr<FloatAttribute>(A_DEPTH_MAP_EXPONENT),
             .talus_limit = talus_limit,
             .viscosity_law_power = node.get_attr<FloatAttribute>(A_VISCOSITY_LAW_POWER),
             .nx_strided = nx_strided,
             .iterations = iterations,
             .solver_stride = stride};
  }();

  // --- Adjust compute mode (stride)

  hmap::ComputeMode cm = node.cfg().cm_gpu;
  cm.stride = params.solver_stride;

  // --- Compute

  hmap::VirtualArray depth_init(CONFIG(node));

  // convect mud field to get its depth
  hmap::for_each_tile(
      {p_in, p_map},
      {p_out, p_depth, &depth_init},
      [&node, &params](std::vector<const hmap::Array *> p_arrays_in,
                       std::vector<hmap::Array *>       p_arrays_out,
                       const hmap::TileRegion &)
      {
        auto [pa_in, pa_map] = unpack<2>(p_arrays_in);
        auto [pa_out, pa_depth, pa_depth_init] = unpack<3>(p_arrays_out);

        *pa_out = *pa_in;

        if (pa_map)
        {
          hmap::gpu::mudslide(*pa_out,
                              *pa_map,
                              params.depth,
                              params.iterations,
                              params.depth_map_exponent,
                              params.viscosity_law_power,
                              pa_depth,
                              pa_depth_init);
        }
        else
        {
          hmap::gpu::mudslide(*pa_out,
                              params.talus_limit,
                              params.depth,
                              params.iterations,
                              params.depth_map_exponent,
                              params.viscosity_law_power,
                              pa_depth,
                              pa_depth_init);
        }
      },
      cm);

  // add it to input elevation (made here separetely because of the
  // stride of the computation)
  hmap::for_each_tile(
      {p_in, p_depth, &depth_init},
      {p_out},
      [](std::vector<const hmap::Array *> p_arrays_in,
         std::vector<hmap::Array *>       p_arrays_out,
         const hmap::TileRegion &)
      {
        auto [pa_in, pa_depth, pa_depth_init] = unpack<3>(p_arrays_in);
        auto [pa_out] = unpack<1>(p_arrays_out);

        *pa_out = *pa_in + *pa_depth - *pa_depth_init;
      },
      node.cfg().cm_cpu);

  // --- post-process

  p_out->smooth_overlap_buffers();
  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
