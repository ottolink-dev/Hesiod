/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/kernels.hpp"
#include "highmap/opencl/gpu_opencl.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_expand_shrink_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "mask");
  node.add_port<hmap::Array>(gnode::PortType::IN, "kernel");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<EnumAttribute>("kernel",
                               "kernel",
                               enum_mappings.kernel_type_map,
                               "cubic_pulse");
  node.add_attr<FloatAttribute>("radius", "radius", 0.05f, 0.01f, 0.2f);
  node.add_attr<BoolAttribute>("shrink", "shrink", "shrink", "expand", false);
  node.add_attr<IntAttribute>("iterations", "iterations", 1, 1, 10);

  // attribute(s) order
  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Main Parameters",
                             "kernel",
                             "radius",
                             "shrink",
                             "iterations",
                             "_GROUPBOX_END_"});

  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

void compute_expand_shrink_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>("mask");
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    // prepare mask
    std::shared_ptr<hmap::VirtualArray> sp_mask = pre_process_mask(node, p_mask, *p_in);

    // kernel definition - use input kernel by default, if not switch
    // to built-in kernels
    hmap::Array  kernel_array;
    hmap::Array *p_kernel = node.get_value_ref<hmap::Array>("kernel");

    if (p_kernel)
    {
      kernel_array = *p_kernel;
    }
    else
    {
      int        ir = std::max(1,
                        (int)(node.get_attr<FloatAttribute>("radius") * p_out->shape.x));
      glm::ivec2 kernel_shape = {2 * ir + 1, 2 * ir + 1};

      kernel_array = hmap::get_kernel(
          kernel_shape,
          (hmap::KernelType)node.get_attr<EnumAttribute>("kernel"));
    }

    // core operator
    std::function<void(hmap::Array &, hmap::Array *)> lambda;

    if (node.get_attr<BoolAttribute>("shrink"))
    {
      hmap::for_each_tile(
          {p_out, p_in, p_mask},
          [&node, &kernel_array](std::vector<hmap::Array *> p_arrays,
                                 const hmap::TileRegion &)
          {
            auto [pa_out, pa_in, pa_mask] = unpack<3>(p_arrays);

            *pa_out = *pa_in;

            hmap::gpu::shrink(*pa_out,
                              kernel_array,
                              pa_mask,
                              node.get_attr<IntAttribute>("iterations"));
          },
          node.cfg().cm_gpu);
    }
    else
    {
      hmap::for_each_tile(
          {p_out, p_in, p_mask},
          [&node, &kernel_array](std::vector<hmap::Array *> p_arrays,
                                 const hmap::TileRegion &)
          {
            auto [pa_out, pa_in, pa_mask] = unpack<3>(p_arrays);

            *pa_out = *pa_in;

            hmap::gpu::expand(*pa_out,
                              kernel_array,
                              pa_mask,
                              node.get_attr<IntAttribute>("iterations"));
          },
          node.cfg().cm_gpu);
    }

    p_out->smooth_overlap_buffers();

    // post-process
    post_process_heightmap(node, *p_out, p_in);
  }
}

} // namespace hesiod
