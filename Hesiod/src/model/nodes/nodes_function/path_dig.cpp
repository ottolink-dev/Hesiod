/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/carving.hpp"
#include "highmap/geometry/path.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_path_dig_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Path>(gnode::PortType::IN, "path");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("width", "width", 0.001f, 0.f, 0.1f);
  node.add_attr<FloatAttribute>("decay", "decay", 0.001f, 0.f, 0.1f);
  node.add_attr<FloatAttribute>("flattening_radius",
                                "flattening_radius",
                                0.001f,
                                0.f,
                                0.1f);
  node.add_attr<FloatAttribute>("depth", "depth", 0.f, -0.2f, 0.2f);
  node.add_attr<BoolAttribute>("force_downhill", "force_downhill", false);

  // attribute(s) order
  node.set_attr_ordered_key(
      {"width", "decay", "flattening_radius", "depth", "force_downhill"});
}

void compute_path_dig_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Path         *p_path = node.get_value_ref<hmap::Path>("path");
  hmap::VirtualArray *p_in   = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_path && p_in)
    if (p_path->size() > 1)
    {
      hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

      int ir_width = (int)(node.get_attr<FloatAttribute>("width") * p_out->shape.x);
      int ir_decay = (int)(node.get_attr<FloatAttribute>("decay") * p_out->shape.x);
      int ir_flattening_radius = (int)(node.get_attr<FloatAttribute>(
                                           "flattening_radius") *
                                       p_out->shape.x);

      ir_width             = std::max(1, ir_width);
      ir_decay             = std::max(1, ir_decay);
      ir_flattening_radius = std::max(1, ir_flattening_radius);

      if (!node.get_attr<BoolAttribute>("force_downhill"))
      {
        hmap::for_each_tile(
            {p_out, p_in},
            [&node, p_path, ir_width, ir_decay, ir_flattening_radius](
                std::vector<hmap::Array *> p_arrays,
                const hmap::TileRegion    &region)
            {
              hmap::Array *pa_out = p_arrays[0];
              hmap::Array *pa_in  = p_arrays[1];

              *pa_out = *pa_in;

              hmap::dig_path(*pa_out,
                             *p_path,
                             ir_width,
                             ir_decay,
                             ir_flattening_radius,
                             node.get_attr<BoolAttribute>("force_downhill"),
                             region.bbox,
                             node.get_attr<FloatAttribute>("depth"));
            },
            node.cfg().cm_cpu);
      }
      else
      {
        // TODO if downhill is activated, so far not distributed
        hmap::Array z_array = p_out->to_array(node.cfg().cm_cpu);

        hmap::dig_path(z_array,
                       *p_path,
                       ir_width,
                       ir_decay,
                       ir_flattening_radius,
                       node.get_attr<BoolAttribute>("force_downhill"),
                       glm::vec4(0.f, 1.f, 0.f, 1.f), // bbox
                       node.get_attr<FloatAttribute>("depth"));

        p_out->from_array(z_array, node.cfg().cm_cpu);
      }

      p_out->smooth_overlap_buffers();
    }
}

} // namespace hesiod
