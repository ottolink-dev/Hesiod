/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/kernels.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

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
  node.add_attr<FloatAttribute>(A_RADIUS, "radius", 0.1f, 0.001f, 0.2f);
  node.add_attr<BoolAttribute>(A_NORMALIZE, "normalize", false);
  node.add_attr<BoolAttribute>(A_ENVELOPE, "envelope", false);
  node.add_attr<EnumAttribute>(A_ENVELOPE_KERNEL,
                               "envelope_kernel",
                               enum_mappings.kernel_type_map,
                               "cubic_pulse");
  // clang-format on

  // --- Attribute(s) order

  node.set_attr_ordered_key(
      {A_RADIUS, A_NORMALIZE, "_SEPARATOR_", A_ENVELOPE, A_ENVELOPE_KERNEL});
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
  const auto radius          = node.get_attr<FloatAttribute>(A_RADIUS);
  const auto normalize       = node.get_attr<BoolAttribute>(A_NORMALIZE);
  const auto envelope        = node.get_attr<BoolAttribute>(A_ENVELOPE);
  const auto envelope_kernel = static_cast<hmap::KernelType>(node.get_attr<EnumAttribute>(A_ENVELOPE_KERNEL));
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
