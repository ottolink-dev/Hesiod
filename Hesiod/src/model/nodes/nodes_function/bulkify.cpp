/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN   = "input";
constexpr const char *P_DX   = "dx";
constexpr const char *P_DY   = "dy";
constexpr const char *P_OUT  = "output";

constexpr const char *A_AMPLITUDE = "amplitude";
constexpr const char *A_BULK_TYPE = "bulk_type";
constexpr const char *A_CENTER    = "center";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_bulkify_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  node.set_current_category("Bulk Shape");
  add_float(node, A_AMPLITUDE, "amplitude", 1.f, -1.f, 4.f);
  add_enum(node, A_BULK_TYPE, "bulk_type", enum_mappings.primitive_type_map);

  node.set_current_category("Position");
  add_xy(node, A_CENTER, "center");

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_bulkify_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in  = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);
  auto *p_dx  = node.get_value_ref<hmap::VirtualArray>(P_DX);
  auto *p_dy  = node.get_value_ref<hmap::VirtualArray>(P_DY);

  if (!p_in || !p_out)
    return;

  // --- Params

  const auto amplitude = node.val<float>(A_AMPLITUDE);
  const auto bulk_type = static_cast<hmap::PrimitiveType>(node.val<int>(A_BULK_TYPE));
  const auto center    = node.val<glm::vec2>(A_CENTER);

  // --- Compute

  hmap::for_each_tile(
      {p_out, p_in, p_dx, p_dy},
      [&](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
      {
        auto [pa_out, pa_in, pa_dx, pa_dy] = unpack<4>(p_arrays);

        *pa_out = hmap::bulkify(
            *pa_in,
            bulk_type,
            amplitude,
            pa_dx,
            pa_dy,
            center,
            region.bbox);
      },
      node.cfg().cm_cpu);

  p_out->remap(p_in->min(node.cfg().cm_cpu),
               p_in->max(node.cfg().cm_cpu),
               node.cfg().cm_cpu);

  // --- Post-process

  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
