/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/range.hpp"

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
constexpr const char *P_INPUT_1 = "input 1";
constexpr const char *P_INPUT_2 = "input 2";
constexpr const char *P_OUT     = "output";

constexpr const char *A_METHOD      = "method";
constexpr const char *A_SWAP_INPUTS = "swap_inputs";

void setup_combine_mask_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_INPUT_1);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_INPUT_2);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_enum(node,
           A_METHOD,
           "method",
           enum_mappings.mask_combine_method_map,
           "intersection");
  add_bool(node, A_SWAP_INPUTS, "swap_inputs", false);

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_combine_mask_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in1 = node.get_value_ref<hmap::VirtualArray>(P_INPUT_1);
  hmap::VirtualArray *p_in2 = node.get_value_ref<hmap::VirtualArray>(P_INPUT_2);

  if (!p_in1 || !p_in2)
    return;

  // adjust inputs
  if (node.val<bool>(A_SWAP_INPUTS))
    std::swap(p_in1, p_in2);

  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  std::function<void(std::vector<const hmap::Array *> p_arrays_in,
                     std::vector<hmap::Array *>       p_arrays_out,
                     const hmap::TileRegion &)>
      lambda;

  const auto method = node.val_enum<MaskCombineMethod>(A_METHOD);

  switch (method)
  {
  case MaskCombineMethod::UNION:
    lambda = [](std::vector<const hmap::Array *> p_arrays_in,
                std::vector<hmap::Array *>       p_arrays_out,
                const hmap::TileRegion &)
    {
      auto [pa_in1, pa_in2] = unpack<2>(p_arrays_in);
      auto [pa_out]         = unpack<1>(p_arrays_out);
      *pa_out               = hmap::maximum(*pa_in1, *pa_in2);
    };
    break;

  case MaskCombineMethod::INTERSECTION:
    lambda = [](std::vector<const hmap::Array *> p_arrays_in,
                std::vector<hmap::Array *>       p_arrays_out,
                const hmap::TileRegion &)
    {
      auto [pa_in1, pa_in2] = unpack<2>(p_arrays_in);
      auto [pa_out]         = unpack<1>(p_arrays_out);
      *pa_out               = hmap::minimum(*pa_in1, *pa_in2);
    };
    break;

  case MaskCombineMethod::EXCLUSION:
    lambda = [](std::vector<const hmap::Array *> p_arrays_in,
                std::vector<hmap::Array *>       p_arrays_out,
                const hmap::TileRegion &)
    {
      auto [pa_in1, pa_in2] = unpack<2>(p_arrays_in);
      auto [pa_out]         = unpack<1>(p_arrays_out);
      *pa_out               = *pa_in1 - *pa_in2;
      hmap::clamp_min(*pa_out, 0.f);
    };
    break;
  }

  hmap::for_each_tile({p_in1, p_in2}, {p_out}, lambda, node.cfg().cm_cpu);

  // post-process
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
