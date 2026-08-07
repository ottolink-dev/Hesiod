/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/carving.hpp"
#include "highmap/geometry/path.hpp"

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
constexpr const char *P_DR   = "dr";
constexpr const char *P_IN   = "input";
constexpr const char *P_MASK = "mask";
constexpr const char *P_OUT  = "output";
constexpr const char *P_PATH = "path";

constexpr const char *A_BOTTOM_EXTENT            = "bottom_extent";
constexpr const char *A_DEPTH                    = "depth";
constexpr const char *A_FALLOFF_DISTANCE_RATIO   = "falloff_distance_ratio";
constexpr const char *A_OUTER_SLOPE              = "outer_slope";
constexpr const char *A_PRESERVE_BEDSHAPE        = "preserve_bedshape";
constexpr const char *A_RADIAL_PROFILE           = "radial_profile";
constexpr const char *A_RADIAL_PROFILE_PARAMETER = "radial_profile_parameter";
constexpr const char *A_VMIN                     = "vmin";

void setup_flatbed_carve_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::Path>(gnode::PortType::IN, P_PATH);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DR);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_MASK, CONFIG(node));

  // attribute(s)
  add_float(node, A_BOTTOM_EXTENT, "Bed Half-Width", 0.02f, 0.f, 0.2f);
  add_float(node, A_VMIN, "Bed Base Height", 0.f, -1.f, 1.f);
  add_float(node, A_DEPTH, "Bed Depth", 0.02f, 0.f, 0.1f);
  add_float(node, A_FALLOFF_DISTANCE_RATIO, "Falloff Width Ratio", 4.f, 0.f, 10.f);
  add_float(node, A_OUTER_SLOPE, "Outer Linear Slope", 0.1f, 0.f, FLT_MAX);
  add_bool(node, A_PRESERVE_BEDSHAPE, "Preserve Bed Shape", true);
  add_enum(node,
           A_RADIAL_PROFILE,
           "Profile Type",
           enum_mappings.radial_profile_map,
           "Smoothstep");
  add_float(node, A_RADIAL_PROFILE_PARAMETER, "Profile Sharpness", 2.f, 0.f, 8.f);
}

void compute_flatbed_carve_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Path         *p_path = node.get_value_ref<hmap::Path>(P_PATH);
  hmap::VirtualArray *p_in   = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_path && p_in)
    if (p_path->size() > 1)
    {
      hmap::VirtualArray *p_out  = node.get_value_ref<hmap::VirtualArray>(P_OUT);
      hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);
      hmap::VirtualArray *p_dr   = node.get_value_ref<hmap::VirtualArray>(P_DR);

      float width   = node.val<float>(A_BOTTOM_EXTENT) * p_out->shape.x;
      float falloff = width * node.val<float>(A_FALLOFF_DISTANCE_RATIO);

      hmap::for_each_tile(
          {p_out, p_in, p_dr, p_mask},
          [&node, width, falloff, p_path](std::vector<hmap::Array *> p_arrays,
                                          const hmap::TileRegion    &region)
          {
            hmap::Array *pa_out  = p_arrays[0];
            hmap::Array *pa_in   = p_arrays[1];
            hmap::Array *pa_dr   = p_arrays[2];
            hmap::Array *pa_mask = p_arrays[3];

            *pa_out = *pa_in;

            hmap::flatbed_carve(*pa_out,
                                *p_path,
                                width,
                                node.val<float>(A_VMIN),
                                node.val<float>(A_DEPTH),
                                falloff,
                                node.val<float>(A_OUTER_SLOPE),
                                node.val<bool>(A_PRESERVE_BEDSHAPE),
                                (hmap::RadialProfile)node.val<int>(A_RADIAL_PROFILE),
                                node.val<float>(A_RADIAL_PROFILE_PARAMETER),
                                pa_mask,
                                pa_dr,
                                region.bbox);
          },
          node.cfg().cm_cpu);

      p_out->smooth_overlap_buffers();
      p_mask->smooth_overlap_buffers();
    }
}

} // namespace hesiod
