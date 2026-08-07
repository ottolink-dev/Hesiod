/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/authoring.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_ridgelines_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Path>(gnode::PortType::IN, "path");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dx");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dy");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "heightmap", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("talus_global", "talus_global", 4.f, -FLT_MAX, FLT_MAX);
  node.add_attr<FloatAttribute>("k_smoothing", "k_smoothing", 1.f, 0.01f, 2.f);
  node.add_attr<FloatAttribute>("width", "width", 0.1f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("vmin", "vmin", 0.f, -1.f, 1.f);
}

void compute_ridgelines_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Path *p_path = node.get_value_ref<hmap::Path>("path");

  if (p_path)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("heightmap");

    if (p_path->size() > 1)
    {
      hmap::VirtualArray *p_dx = node.get_value_ref<hmap::VirtualArray>("dx");
      hmap::VirtualArray *p_dy = node.get_value_ref<hmap::VirtualArray>("dy");

      std::vector<float> xs, ys, zs = {};

      for (size_t k = 0; k < p_path->size() - 1; k++)
      {
        xs.push_back(p_path->points[k].x);
        ys.push_back(p_path->points[k].y);
        zs.push_back(p_path->points[k].v);

        xs.push_back(p_path->points[k + 1].x);
        ys.push_back(p_path->points[k + 1].y);
        zs.push_back(p_path->points[k + 1].v);
      }

      hmap::for_each_tile(
          {p_out, p_dx, p_dy},
          [&node, p_path, xs, ys, zs](std::vector<hmap::Array *> p_arrays,
                                      const hmap::TileRegion    &region)
          {
            hmap::Array *pa_out = p_arrays[0];
            hmap::Array *pa_dx  = p_arrays[1];
            hmap::Array *pa_dy  = p_arrays[2];

            glm::vec4 bbox_points = {0.f, 1.f, 0.f, 1.f};

            *pa_out = hmap::ridgelines(region.shape,
                                       xs,
                                       ys,
                                       zs,
                                       node.get_attr<FloatAttribute>("talus_global"),
                                       node.get_attr<FloatAttribute>("k_smoothing"),
                                       node.get_attr<FloatAttribute>("width"),
                                       node.get_attr<FloatAttribute>("vmin"),
                                       bbox_points,
                                       pa_dx,
                                       pa_dy,
                                       nullptr, // stretching
                                       region.bbox);
          },
          node.cfg().cm_cpu);
    }
    else
    {
      // fill with zeros
      hmap::for_each_tile(
          {p_out},
          [](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
          {
            hmap::Array *pa_out = p_arrays[0];
            *pa_out             = 0.f;
          },
          node.cfg().cm_cpu);
    }
  }
}

} // namespace hesiod
