/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/kernels.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_KERNEL = "kernel";

constexpr const char *A_KERNEL    = "kernel";
constexpr const char *A_NORMALIZE = "normalize";
constexpr const char *A_RADIUS    = "radius";

void setup_kernel_prim_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Array>(gnode::PortType::OUT, P_KERNEL, node.cfg().shape);

  // attribute(s)
  add_enum(node, A_KERNEL, "kernel", enum_mappings.kernel_type_map, "cubic_pulse");
  add_float(node, A_RADIUS, "radius", 0.1f, 0.001f, 0.2f);
  add_bool(node, A_NORMALIZE, "normalize", false);
}

void compute_kernel_prim_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Array *p_out = node.get_value_ref<hmap::Array>(P_KERNEL);

  int ir = std::max(1, (int)(node.val<float>(A_RADIUS) * node.cfg().shape.x));

  // kernel definition
  glm::ivec2 kernel_shape = {2 * ir + 1, 2 * ir + 1};

  *p_out = hmap::get_kernel(kernel_shape, (hmap::KernelType)node.val<int>(A_KERNEL));

  if (node.val<bool>(A_NORMALIZE))
    *p_out /= p_out->sum();
}

} // namespace hesiod
