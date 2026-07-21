/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/carving.hpp"
#include "highmap/geometry/path.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_flatbed_carve_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::Path>(gnode::PortType::IN, "path");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dr");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "mask", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("bottom_extent", "Bed Half-Width", 0.02f, 0.f, 0.2f);
  node.add_attr<FloatAttribute>("vmin", "Bed Base Height", 0.f, -1.f, 1.f);
  node.add_attr<FloatAttribute>("depth", "Bed Depth", 0.02f, 0.f, 0.1f);
  node.add_attr<FloatAttribute>("falloff_distance_ratio",
                                "Falloff Width Ratio",
                                4.f,
                                0.f,
                                10.f);
  node.add_attr<FloatAttribute>("outer_slope", "Outer Linear Slope", 0.1f, 0.f, FLT_MAX);
  node.add_attr<BoolAttribute>("preserve_bedshape", "Preserve Bed Shape", true);
  node.add_attr<EnumAttribute>("radial_profile",
                               "Profile Type",
                               enum_mappings.radial_profile_map,
                               "Smoothstep");
  node.add_attr<FloatAttribute>("radial_profile_parameter",
                                "Profile Sharpness",
                                2.f,
                                0.f,
                                8.f);

  // attribute(s) order
  node.set_attr_ordered_key({
      "_GROUPBOX_BEGIN_Bed Geometry",
      "bottom_extent",
      "depth",
      "vmin",
      "_GROUPBOX_END_",
      //
      "_GROUPBOX_BEGIN_Falloff and Blending",
      "falloff_distance_ratio",
      "outer_slope",
      "_GROUPBOX_END_",
      //
      "_GROUPBOX_BEGIN_Radial Profile",
      "radial_profile",
      "radial_profile_parameter",
      "_GROUPBOX_END_",
      //
      "_GROUPBOX_BEGIN_Shape Preservation",
      "preserve_bedshape",
      "_GROUPBOX_END_",
  });
}

void compute_flatbed_carve_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Path         *p_path = node.get_value_ref<hmap::Path>("path");
  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_path && p_in)
    if (p_path->size() > 1)
    {
      hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");
      hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>("mask");
      hmap::VirtualArray *p_dr = node.get_value_ref<hmap::VirtualArray>("dr");

      float width = node.get_attr<FloatAttribute>("bottom_extent") * p_out->shape.x;
      float falloff = width * node.get_attr<FloatAttribute>("falloff_distance_ratio");

      hmap::for_each_tile(
          {p_out, p_in, p_dr, p_mask},
          [&node, width, falloff, p_path](std::vector<hmap::Array *> p_arrays,
                                          const hmap::TileRegion    &region)
          {
            hmap::Array *pa_out = p_arrays[0];
            hmap::Array *pa_in = p_arrays[1];
            hmap::Array *pa_dr = p_arrays[2];
            hmap::Array *pa_mask = p_arrays[3];

            *pa_out = *pa_in;

            hmap::flatbed_carve(
                *pa_out,
                *p_path,
                width,
                node.get_attr<FloatAttribute>("vmin"),
                node.get_attr<FloatAttribute>("depth"),
                falloff,
                node.get_attr<FloatAttribute>("outer_slope"),
                node.get_attr<BoolAttribute>("preserve_bedshape"),
                (hmap::RadialProfile)node.get_attr<EnumAttribute>("radial_profile"),
                node.get_attr<FloatAttribute>("radial_profile_parameter"),
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
