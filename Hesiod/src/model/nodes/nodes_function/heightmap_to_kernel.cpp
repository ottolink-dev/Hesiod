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

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_HEIGHTMAP = "heightmap";
constexpr const char *P_KERNEL    = "kernel";

constexpr const char *A_RADIUS          = "radius";
constexpr const char *A_NORMALIZE       = "normalize";
constexpr const char *A_ENVELOPE        = "envelope";
constexpr const char *A_ENVELOPE_KERNEL = "envelope_kernel";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_heightmap_to_kernel_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_HEIGHTMAP);
  node.add_port<hmap::Array>(gnode::PortType::OUT, P_KERNEL, node.cfg().shape);

  // --- Attributes

  // clang-format off
  add_float(node, A_RADIUS, "radius", 0.1f, 0.001f, 0.2f);
  add_bool(node, A_NORMALIZE, "normalize", false);
  add_bool(node, A_ENVELOPE, "envelope", false);
  add_enum(node, A_ENVELOPE_KERNEL, "envelope_kernel", enum_mappings.kernel_type_map, "cubic_pulse");
  // clang-format on

  // --- Attribute(s) order
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_heightmap_to_kernel_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in  = node.get_value_ref<hmap::VirtualArray>(P_HEIGHTMAP);
  auto *p_out = node.get_value_ref<hmap::Array>(P_KERNEL);

  if (!p_in || !p_out)
    return;

  // --- Params

  // clang-format off
  const auto radius          = node.val<float>(A_RADIUS);
  const auto normalize       = node.val<bool>(A_NORMALIZE);
  const auto envelope        = node.val<bool>(A_ENVELOPE);
  const auto envelope_kernel = static_cast<hmap::KernelType>(node.val<int>(A_ENVELOPE_KERNEL));
  // clang-format on

  const int        ir = std::max(1, static_cast<int>(radius * node.cfg().shape.x));
  const glm::ivec2 kernel_shape = {2 * ir + 1, 2 * ir + 1};

  // --- Compute

  *p_out = p_in->to_array(kernel_shape, node.cfg().cm_cpu);

  if (envelope)
  {
    hmap::Array env = hmap::get_kernel(kernel_shape, envelope_kernel);
    *p_out *= env;
  }

  if (normalize)
  {
    float sum = p_out->sum();
    if (sum)
      *p_out /= sum;
  }
}

} // namespace hesiod
