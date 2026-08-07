/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/hydrology/hydrology.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_water_depth_from_mask_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "elevation");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "water_mask");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "water_depth", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("mask_threshold",
                                "Mask Activation Threshold",
                                0.01f,
                                0.f,
                                1.f,
                                "{:.3f}");
  node.add_attr<FloatAttribute>("tolerance",
                                "Convergence Tolerance",
                                1e-6f,
                                1e-8f,
                                1e-4f,
                                "{:.3e}",
                                true);
  node.add_attr<IntAttribute>("iterations", "Max Iterations", 500, 1, INT_MAX);
  node.add_attr<FloatAttribute>("omega", "Relaxation Factor", 1.8f, 1e-3f, 1.9f);

  // attribute(s) order
  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Interpolation Domain",
                             "mask_threshold",
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Solver Parameters",
                             "tolerance",
                             "iterations",
                             "omega",
                             "_GROUPBOX_END_"});
}

void compute_water_depth_from_mask_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_z    = node.get_value_ref<hmap::VirtualArray>("elevation");
  hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>("water_mask");

  if (p_z && p_mask)
  {
    hmap::VirtualArray *p_depth = node.get_value_ref<hmap::VirtualArray>("water_depth");

    hmap::for_each_tile(
        {p_depth, p_z, p_mask},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          hmap::Array *pa_depth = p_arrays[0];
          hmap::Array *pa_z     = p_arrays[1];
          hmap::Array *pa_mask  = p_arrays[2];

          *pa_depth = hmap::water_depth_from_mask(
              *pa_z,
              *pa_mask,
              node.get_attr<FloatAttribute>("mask_threshold"),
              node.get_attr<IntAttribute>("iterations"),
              node.get_attr<FloatAttribute>("tolerance"),
              node.get_attr<FloatAttribute>("omega"));
        },
        node.cfg().cm_cpu);

    p_depth->smooth_overlap_buffers();
  }
}

} // namespace hesiod
