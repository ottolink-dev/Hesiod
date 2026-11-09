/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/local_metrics.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/range.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "output";

constexpr const char *A_RADIUS    = "radius";
constexpr const char *A_NORMALIZE = "normalize";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_rugosity_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  add_float(node, A_RADIUS, "radius", 0.1f, 0.f, 0.5f);
  add_bool(node, A_NORMALIZE, "Normalize", true);

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_rugosity_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in  = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in)
    return;

  // --- Params

  const auto ir        = node.val_pixel_radius(A_RADIUS);
  const auto normalize = node.val<bool>(A_NORMALIZE);

  // --- Compute

  hmap::for_each_tile(
      {p_in},
      {p_out},
      [&node, ir](std::vector<const hmap::Array *> p_arrays_in,
                  std::vector<hmap::Array *>       p_arrays_out,
                  const hmap::TileRegion &)
      {
        auto [pa_in]  = unpack<1>(p_arrays_in);
        auto [pa_out] = unpack<1>(p_arrays_out);
        *pa_out       = hmap::gpu::rugosity(*pa_in, ir);
      },
      node.cfg().cm_gpu);

  p_out->sync_overlap_buffers();

  // --- Post-process

  if (normalize)
  {
    hmap::for_each_tile(
        {},
        {p_out},
        [&](std::vector<const hmap::Array *>,
            std::vector<hmap::Array *> p_arrays_out,
            const hmap::TileRegion &,
            hmap::ICommunicator &comm)
        {
          auto [pa_out] = unpack<1>(p_arrays_out);
          hmap::normalize_and_clip(*pa_out, 2.5f, true, true, comm);
        },
        node.cfg().cm_cpu);
  }

  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
