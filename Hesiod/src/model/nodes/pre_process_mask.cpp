/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/gradient.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/range.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/model/nodes/base_node.hpp"

using namespace attr;

namespace hesiod
{

std::shared_ptr<hmap::VirtualArray> pre_process_mask(BaseNode            &node,
                                                     hmap::VirtualArray *&p_mask,
                                                     hmap::VirtualArray  &h)
{
  // do not modify any existing input mask and return a dummy shared
  // pointer that will not be used
  if (p_mask || !node.get_attr<BoolAttribute>("mask_activate"))
    return std::make_shared<hmap::VirtualArray>();

  // create mask storage and assign to current mask pointer
  std::shared_ptr<hmap::VirtualArray> sp_mask = std::make_shared<hmap::VirtualArray>(
      node.cfg().shape,
      node.cfg().tile_shape,
      node.cfg().halo,
      node.cfg().storage_mode);
  p_mask = sp_mask.get();

  const GraphConfig &cfg = *node.get_config_ref();

  // --- mask definition

  const std::string mask_type = node.get_attr<ChoiceAttribute>("mask_type");
  const int ir = (int)(node.get_attr<FloatAttribute>("mask_radius") * p_mask->shape.x);

  if (mask_type == "Elevation")
  {
    hmap::copy_data(h, *p_mask, cfg.cm_cpu);
    p_mask->remap(0.f, 1.f, cfg.cm_cpu);
  }
  else if (mask_type == "Elevation mid-range")
  {
    hmap::copy_data(h, *p_mask, cfg.cm_cpu);
    p_mask->remap(0.f, 1.f, cfg.cm_cpu);

    // mask <- h * (1 - h)
    hmap::for_each_tile(
        {p_mask},
        [](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          hmap::Array *pa_out = p_arrays[0];
          *pa_out *= (1.f - *pa_out);
        },
        cfg.cm_cpu);
  }
  else if (mask_type == "Gradient norm")
  {
    hmap::for_each_tile(
        {p_mask, &h},
        [](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);

          *pa_out = hmap::gradient_norm(*pa_in);
        },
        cfg.cm_cpu);

    p_mask->remap(0.f, 1.f, cfg.cm_cpu);
  }
  else
  {
    Logger::log()->error("pre_process_mask: unknown mask type: {}", mask_type);
  }

  // filter
  if (ir > 0)
  {
    hmap::for_each_tile(
        {p_mask},
        [&ir](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          hmap::Array *pa_out = p_arrays[0];
          hmap::gpu::smooth_cpulse(*pa_out, ir);
        },
        cfg.cm_gpu);

    p_mask->smooth_overlap_buffers();
    p_mask->remap(0.f, 1.f, cfg.cm_cpu);
  }

  // --- apply gain to the mask

  float mask_gain = node.get_attr<FloatAttribute>("mask_gain");

  if (mask_gain != 1.f)
  {
    hmap::for_each_tile(
        {p_mask},
        [mask_gain](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          hmap::Array *pa = p_arrays[0];
          hmap::gain(*pa, mask_gain);
        },
        cfg.cm_cpu);
  }

  if (node.get_attr<BoolAttribute>("mask_inverse"))
    p_mask->inverse(cfg.cm_cpu);

  return sp_mask;
}

void setup_pre_process_mask_attributes(BaseNode &node)
{
  node.add_attr<BoolAttribute>("mask_activate", "mask_activate", false);

  std::vector<std::string> choices = {"Elevation",
                                      "Elevation mid-range",
                                      "Gradient norm"};
  node.add_attr<ChoiceAttribute>("mask_type", "mask_type", choices);

  node.add_attr<BoolAttribute>("mask_inverse", "mask_inverse", false);

  node.add_attr<FloatAttribute>("mask_radius", "mask_radius", 0.01f, 0.f, 0.2f);

  node.add_attr<FloatAttribute>("mask_gain", "mask_gain", 1.f, 0.f, 10.f);

  std::vector<std::string> *p_keys = node.get_attr_ordered_key_ref();

  p_keys->push_back("_GROUPBOX_BEGIN_Mask Definition");
  p_keys->push_back("mask_activate");
  p_keys->push_back("mask_type");
  p_keys->push_back("mask_radius");
  p_keys->push_back("mask_gain");
  p_keys->push_back("mask_inverse");
  p_keys->push_back("_GROUPBOX_END_");
}

} // namespace hesiod
