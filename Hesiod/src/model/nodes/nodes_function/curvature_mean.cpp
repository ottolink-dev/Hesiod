/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/curvature.hpp"
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

constexpr const char *A_CLAMP_MAX   = "clamp_max";
constexpr const char *A_VALUES_KEPT = "values_kept";
constexpr const char *A_VC_MAX      = "vc_max";

void setup_curvature_mean_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  std::vector<std::string> choices = {"positive", "negative", "both"};
  add_choice(node, A_VALUES_KEPT, "values_kept", choices);
  add_bool(node, A_CLAMP_MAX, "clamp_max", false);
  add_float(node, A_VC_MAX, "vc_max", 1.f, 0.f, FLT_MAX, "{:.4f}");

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

void compute_curvature_mean_node(BaseNode &node)
{
  Logger::log()->error("CurvatureMean node is deprecated, use Curvatures node");

  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    int nx = p_out->shape.x; // for gradient scaling

    hmap::for_each_tile(
        {p_out, p_in},
        [&node, nx](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          // compute mean curvature and scale it
          *pa_out = hmap::gpu::curvature_quadric(*pa_in,
                                                 0,
                                                 hmap::CurvatureType::CT_MEAN) *
                    nx;

          // determine curvature sign handling
          const std::string choice    = node.val<std::string>(A_VALUES_KEPT);
          const bool        keep_both = (choice == "both");

          // if only one curvature sign is kept
          if (!keep_both)
          {
            if (choice == "negative")
              *pa_out *= -1.f;

            hmap::clamp_min(*pa_out, 0.f);
          }

          // clamp output range
          const float vc_max = node.val<float>(A_VC_MAX);

          if (node.val<bool>(A_CLAMP_MAX) && !keep_both)
            hmap::clamp_max(*pa_out, vc_max);
          else
            hmap::clamp(*pa_out, -vc_max, vc_max);
        },
        node.cfg().cm_gpu);

    p_out->smooth_overlap_buffers();

    // post-process
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
