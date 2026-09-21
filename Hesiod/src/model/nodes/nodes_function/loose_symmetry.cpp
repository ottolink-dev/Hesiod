/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/synthesis.hpp"
#include "highmap/transform.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "output";

constexpr const char *A_FACTOR        = "factor";
constexpr const char *A_PATCH_RADIUS  = "patch_radius";
constexpr const char *A_SPARSITY      = "sparsity";
constexpr const char *A_STRENGTH      = "strength";
constexpr const char *A_SYMMETRY_TYPE = "symmetry_type";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_loose_symmetry_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  node.set_current_category("Symmetry");
  add_enum(node,
           A_SYMMETRY_TYPE,
           "Symmetry Type",
           enum_mappings.symmetry_type_map,
           "Left to Right");
  add_float(node, A_STRENGTH, "Strength", 1.f, 0.f, 1.f);
  add_int(node, A_FACTOR, "Factor", 4, 1, 32);

  node.set_current_category("Synthesis");
  add_float(node, A_PATCH_RADIUS, "Patch Radius", 0.05f, 0.f, 1.f);
  add_int(node, A_SPARSITY, "Sparsity", 1, 1, 16);

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_loose_symmetry_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  hmap::VirtualArray *p_in  = node.get_value_ref<hmap::VirtualArray>(P_IN);
  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in)
    return;

  // --- Params

  int patch_size       = node.val_pixel_radius(A_PATCH_RADIUS, 2);
  int analysis_stride  = std::max(1, patch_size / 8);
  int synthesis_stride = std::max(1, patch_size / 2);

  // --- Work on a single array (i.e. not-tiled algo)

  hmap::Array in_array = p_in->to_array(node.cfg().cm_cpu);

  hmap::Array out_array = hmap::loose_symmetry(
      in_array,
      static_cast<hmap::SymmetryType>(node.val<int>(A_SYMMETRY_TYPE)),
      node.val<float>(A_STRENGTH),
      node.val<int>(A_FACTOR),
      patch_size,
      analysis_stride,
      synthesis_stride,
      node.val<int>(A_SPARSITY));

  p_out->from_array(out_array, node.cfg().cm_cpu);

  // --- Post-process

  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
