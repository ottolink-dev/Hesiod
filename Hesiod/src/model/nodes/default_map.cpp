/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/primitives.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

void generate_map(BaseNode                &node,
                  hmap::VirtualArray     *&p_map,
                  hmap::VirtualArray      &map,
                  const DefaultMapOptions &options)
{
  // generate
  hmap::for_each_tile(
      {&map},
      [&node, options](std::vector<hmap::Array *> p_arrays,
                       const hmap::TileRegion    &region)
      {
        auto [pa_map] = unpack<1>(p_arrays);

        switch (options.map_type)
        {
        case DefaultMapOptions::Type::DMT_UNIFORM_ONE:
        {
          *pa_map = 1.f;
        }
        break;
          //
        case DefaultMapOptions::Type::DMT_PULSE:
        {
          *pa_map = hmap::cubic_pulse(region.shape,
                                      /* p_noise_x */ nullptr,
                                      /* p_noise_y */ nullptr,
                                      /* center */ {0.5f, 0.5f},
                                      region.bbox);
        }
        break;
          //
        }
      },
      node.cfg().cm_cpu);

  // post-process
  if (options.map_type != DefaultMapOptions::Type::DMT_UNIFORM_ONE)
  {
    float vmin = options.inverse_output ? 1.f : 0.f;
    float vmax = 1.f - vmin;
    map.remap(vmin, vmax, node.cfg().cm_cpu);
  }

  // assign
  p_map = &map;
}

} // namespace hesiod
