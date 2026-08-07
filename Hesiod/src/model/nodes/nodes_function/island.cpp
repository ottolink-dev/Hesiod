/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/primitives.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_island_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "land_mask");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dr");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "out", CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "water_depth", CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "inland_mask", CONFIG(node));

  // attribute(s)
  glm::vec2 kw = {4.f, 4.f};

  node.add_attr<SeedAttribute>("seed", "Seed");
  node.add_attr<FloatAttribute>("noise_amp", "Amplitude", 0.07f, 0.f, 2.f);
  node.add_attr<WaveNbAttribute>("noise_kw", "Spatial Frequency", kw, 0.f, FLT_MAX, true);
  node.add_attr<IntAttribute>("noise_octaves", "Octaves", 8, INT_MAX);
  node.add_attr<FloatAttribute>("noise_rugosity", "Rugosity", 0.7f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("noise_angle", "Direction Angle", 45.f, -180.f, 180.f);
  node.add_attr<FloatAttribute>("noise_k_smoothing",
                                "Smoothing Coefficient",
                                0.05f,
                                0.f,
                                1.f);
  node.add_attr<FloatAttribute>("elevation_scale",
                                "Elevation Scaling Factor",
                                0.4f,
                                0.f,
                                2.f);
  node.add_attr<FloatAttribute>("filter_radius", "Smoothing Radius", 0.01f, 0.f, 0.2f);
  node.add_attr<FloatAttribute>("slope_min", "Lower Slope Limit", 0.05f, 0.f, 32.f);
  node.add_attr<FloatAttribute>("slope_max", "Upper Slope Limit", 1.5f, 0.f, 32.f);
  node.add_attr<FloatAttribute>("slope_start", "Shoreline Slope", 0.5f, 0.f, 4.f);
  node.add_attr<FloatAttribute>("slope_end", "Apex Slope", 1.5f, 0.f, 4.f);
  node.add_attr<FloatAttribute>("slope_noise_intensity",
                                "Slope Noise Intensity",
                                0.f,
                                0.f,
                                100.f);
  node.add_attr<FloatAttribute>("k_smooth", "Smoothing Coefficient", 0.05f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("radial_noise_intensity",
                                "Radial Noise Intensity",
                                0.5f,
                                0.f,
                                2.f);
  node.add_attr<FloatAttribute>("radial_profile_gain",
                                "Radial Profile Gain",
                                1.f,
                                0.01f,
                                10.f);
  node.add_attr<FloatAttribute>("water_decay", "Water Decay Rate", 0.1f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("water_depth", "Water Depth Level", 0.3f, 0.f, FLT_MAX);
  node.add_attr<FloatAttribute>("lee_angle", "Lee-Side Angle", 30.f, -180.f, 180.f);
  node.add_attr<FloatAttribute>("lee_amp", "Lee-Side Amplitude", 0.f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("uplift_amp", "Uplift Amplitude", 0.f, 0.f, 1.f);

  // attribute(s) order
  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Core Parameters",
                             "elevation_scale",
                             "filter_radius",
                             "slope_start",
                             "slope_end",
                             "radial_profile_gain",
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Large-Scale Deformations",
                             "lee_angle",
                             "lee_amp",
                             "uplift_amp",
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Water Parameters",
                             "water_decay",
                             "water_depth",
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Morphology Controls",
                             "slope_min",
                             "slope_max",
                             "slope_noise_intensity",
                             "k_smooth",
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Noise Generation",
                             "radial_noise_intensity",
                             "seed",
                             "noise_amp",
                             "noise_kw",
                             "noise_octaves",
                             "noise_rugosity",
                             "noise_angle",
                             "noise_k_smoothing",
                             "_GROUPBOX_END_"});

  setup_post_process_heightmap_attributes(
      node,
      {.add_mix = false, .remap_active_state = false});

  // disable post-processing remap by default
  node.get_attr_ref<RangeAttribute>("post_remap")->set_is_active(false);
  node.get_attr_ref<RangeAttribute>("post_remap")->save_initial_state();
}

void compute_island_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // base noise function
  hmap::VirtualArray *p_land = node.get_value_ref<hmap::VirtualArray>("land_mask");
  if (!p_land)
    return;

  hmap::VirtualArray *p_dr    = node.get_value_ref<hmap::VirtualArray>("dr");
  hmap::VirtualArray *p_out   = node.get_value_ref<hmap::VirtualArray>("out");
  hmap::VirtualArray *p_depth = node.get_value_ref<hmap::VirtualArray>("water_depth");
  hmap::VirtualArray *p_mask  = node.get_value_ref<hmap::VirtualArray>("inland_mask");

  int ir = (int)(node.get_attr<FloatAttribute>("filter_radius") * p_out->shape.x);

  float scale = node.get_attr<FloatAttribute>("elevation_scale");

  hmap::for_each_tile(
      {p_out, p_land, p_dr, p_depth, p_mask},
      [&node, ir, scale](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
      {
        hmap::Array *pa_out   = p_arrays[0];
        hmap::Array *pa_land  = p_arrays[1];
        hmap::Array *pa_dr    = p_arrays[2];
        hmap::Array *pa_depth = p_arrays[3];
        hmap::Array *pa_mask  = p_arrays[4];

        if (pa_dr)
        {
          *pa_out = hmap::gpu::island(
              *pa_land,
              pa_dr,
              scale,
              (ir != 0),
              ir,
              scale * node.get_attr<FloatAttribute>("slope_min"),
              scale * node.get_attr<FloatAttribute>("slope_max"),
              scale * node.get_attr<FloatAttribute>("slope_start"),
              scale * node.get_attr<FloatAttribute>("slope_end"),
              node.get_attr<FloatAttribute>("slope_noise_intensity"),
              node.get_attr<FloatAttribute>("k_smooth"),
              node.get_attr<FloatAttribute>("radial_noise_intensity"),
              node.get_attr<FloatAttribute>("radial_profile_gain"),
              node.get_attr<FloatAttribute>("water_decay"),
              scale * node.get_attr<FloatAttribute>("water_depth"),
              node.get_attr<FloatAttribute>("lee_angle"),
              scale * node.get_attr<FloatAttribute>("lee_amp"),
              scale * node.get_attr<FloatAttribute>("uplift_amp"),
              pa_depth,
              pa_mask);
        }
        else
        {
          *pa_out = hmap::gpu::island(
              *pa_land,
              node.get_attr<SeedAttribute>("seed"),
              node.get_attr<FloatAttribute>("noise_amp"),
              node.get_attr<WaveNbAttribute>("noise_kw"),
              node.get_attr<IntAttribute>("noise_octaves"),
              node.get_attr<FloatAttribute>("noise_rugosity"),
              node.get_attr<FloatAttribute>("noise_angle"),
              node.get_attr<FloatAttribute>("noise_k_smoothing"),
              scale,
              (ir != 0),
              ir,
              scale * node.get_attr<FloatAttribute>("slope_min"),
              scale * node.get_attr<FloatAttribute>("slope_max"),
              scale * node.get_attr<FloatAttribute>("slope_start"),
              scale * node.get_attr<FloatAttribute>("slope_end"),
              node.get_attr<FloatAttribute>("slope_noise_intensity"),
              node.get_attr<FloatAttribute>("k_smooth"),
              node.get_attr<FloatAttribute>("radial_noise_intensity"),
              node.get_attr<FloatAttribute>("radial_profile_gain"),
              node.get_attr<FloatAttribute>("water_decay"),
              scale * node.get_attr<FloatAttribute>("water_depth"),
              node.get_attr<FloatAttribute>("lee_angle"),
              scale * node.get_attr<FloatAttribute>("lee_amp"),
              scale * node.get_attr<FloatAttribute>("uplift_amp"),
              pa_depth,
              pa_mask);
        }
      },
      node.cfg().cm_gpu);

  // post-process
  p_out->smooth_overlap_buffers();
  p_depth->smooth_overlap_buffers();
  p_mask->smooth_overlap_buffers();

  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
