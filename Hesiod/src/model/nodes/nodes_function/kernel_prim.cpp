/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/kernels.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_kernel_prim_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Array>(gnode::PortType::OUT, "kernel", node.cfg().shape);

  // attribute(s)
  node.add_attr<EnumAttribute>("kernel",
                               "kernel",
                               enum_mappings.kernel_type_map,
                               "cubic_pulse");
  node.add_attr<FloatAttribute>("radius", "radius", 0.1f, 0.001f, 0.2f);
  node.add_attr<BoolAttribute>("normalize", "normalize", false);

  // attribute(s) order
  node.set_attr_ordered_key({"kernel", "radius", "normalize"});
}

void compute_kernel_prim_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Array *p_out = node.get_value_ref<hmap::Array>("kernel");

  int ir = std::max(1,
                    (int)(node.get_attr<FloatAttribute>("radius") * node.cfg().shape.x));

  // kernel definition
  glm::ivec2 kernel_shape = {2 * ir + 1, 2 * ir + 1};

  *p_out = hmap::get_kernel(kernel_shape,
                            (hmap::KernelType)node.get_attr<EnumAttribute>("kernel"));

  if (node.get_attr<BoolAttribute>("normalize"))
    *p_out /= p_out->sum();
}

} // namespace hesiod
