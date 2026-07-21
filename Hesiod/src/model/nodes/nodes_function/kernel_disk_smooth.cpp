/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/kernels.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_kernel_disk_smooth_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Array>(gnode::PortType::OUT, "kernel", node.cfg().shape);

  // attribute(s)
  node.add_attr<FloatAttribute>("radius", "radius", 0.1f, 0.001f, 0.2f);
  node.add_attr<BoolAttribute>("normalize", "normalize", false);
  node.add_attr<FloatAttribute>("r_cutoff", "r_cutoff", 0.9f, 0.f, 1.f);

  // attribute(s) order
  node.set_attr_ordered_key({"radius", "normalize", "_SEPARATOR_", "r_cutoff"});
}

void compute_kernel_disk_smooth_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Array *p_out = node.get_value_ref<hmap::Array>("kernel");

  int ir = std::max(1,
                    (int)(node.get_attr<FloatAttribute>("radius") * node.cfg().shape.x));

  // kernel definition
  glm::ivec2 kernel_shape = {2 * ir + 1, 2 * ir + 1};

  *p_out = hmap::disk_smooth(kernel_shape, node.get_attr<FloatAttribute>("r_cutoff"));

  if (node.get_attr<BoolAttribute>("normalize"))
    *p_out /= p_out->sum();
}

} // namespace hesiod
