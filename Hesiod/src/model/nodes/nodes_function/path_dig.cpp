/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/carving.hpp"
#include "highmap/geometry/path.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_IN   = "input";
constexpr const char *P_OUT  = "output";
constexpr const char *P_PATH = "path";

constexpr const char *A_DECAY             = "decay";
constexpr const char *A_DEPTH             = "depth";
constexpr const char *A_FLATTENING_RADIUS = "flattening_radius";
constexpr const char *A_FORCE_DOWNHILL    = "force_downhill";
constexpr const char *A_WIDTH             = "width";

void setup_path_dig_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Path>(gnode::PortType::IN, P_PATH);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_WIDTH, "width", 0.001f, 0.f, 0.1f);
  add_float(node, A_DECAY, "decay", 0.001f, 0.f, 0.1f);
  add_float(node, A_FLATTENING_RADIUS, "flattening_radius", 0.001f, 0.f, 0.1f);
  add_float(node, A_DEPTH, "depth", 0.f, -0.2f, 0.2f);
  add_bool(node, A_FORCE_DOWNHILL, "force_downhill", false);
}

void compute_path_dig_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Path         *p_path = node.get_value_ref<hmap::Path>(P_PATH);
  hmap::VirtualArray *p_in   = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_path && p_in)
    if (p_path->size() > 1)
    {
      hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

      int ir_width             = (int)(node.val<float>(A_WIDTH) * p_out->shape.x);
      int ir_decay             = (int)(node.val<float>(A_DECAY) * p_out->shape.x);
      int ir_flattening_radius = (int)(node.val<float>(A_FLATTENING_RADIUS) *
                                       p_out->shape.x);

      ir_width             = std::max(1, ir_width);
      ir_decay             = std::max(1, ir_decay);
      ir_flattening_radius = std::max(1, ir_flattening_radius);

      if (!node.val<bool>(A_FORCE_DOWNHILL))
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
                             node.val<bool>(A_FORCE_DOWNHILL),
                             region.bbox,
                             node.val<float>(A_DEPTH));
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
                       node.val<bool>(A_FORCE_DOWNHILL),
                       glm::vec4(0.f, 1.f, 0.f, 1.f), // bbox
                       node.val<float>(A_DEPTH));

        p_out->from_array(z_array, node.cfg().cm_cpu);
      }

      p_out->smooth_overlap_buffers();
    }
}

} // namespace hesiod
