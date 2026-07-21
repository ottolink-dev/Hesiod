/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/operator.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "meta/core/data_provider.hpp"
#include "meta/metadata/keys.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

constexpr const char *P_IN = "input";
constexpr const char *P_OUT = "output";

constexpr const char *A_K_SMOOTHING = "k_smoothing";
constexpr const char *A_RANGE = "range";

void setup_saturate_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  auto &c = node.meta_group().current();

  // k_smoothing
  {
    auto *a = c.add<float>(A_K_SMOOTHING, 0.1f);
    a->metadata().try_add(meta::keys::ui::label, std::string("k_smoothing"));
    a->metadata().try_add(meta::keys::constraints::min, 0.01f);
    a->metadata().try_add(meta::keys::constraints::max, 1.f);
    a->metadata().try_add(meta::keys::ui::category, std::string("Main"));
  }

  // range
  {
    auto *a = c.add<glm::vec2>(A_RANGE, glm::vec2(0.f, 1.f));
    a->metadata().try_add(meta::keys::ui::widget_type, std::string("RangeBar"));
    a->metadata().try_add(meta::keys::constraints::min, -1.f); // legacy RangeAttribute domain
    a->metadata().try_add(meta::keys::constraints::max, 2.f);
    a->metadata().try_add(meta::keys::ui::category, std::string("Main"));
    a->metadata().try_add(meta::keys::ui::tooltip, std::string("<b>Saturation range</b>"));
    a->metadata().try_add(
        meta::keys::ui::data_provider,
        meta::DataProvider{
            [&node, port_id = std::string(P_IN)]() -> meta::ProviderData
            {
              meta::ProviderData d;
              hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(port_id);
              if (!p_in)
                return d;
              float vmin = p_in->min(node.cfg().cm_cpu);
              float vmax = p_in->max(node.cfg().cm_cpu);
              if (vmin == vmax)
                return d;
              const int   nbins = 256;
              hmap::Array arr = p_in->to_array({256, 256}, node.cfg().cm_cpu);
              d.series_x = hmap::linspace(vmin, vmax, nbins, false);
              d.series_y.assign(nbins, 0.f);
              const float sa = 1.f / (vmax - vmin) * (nbins - 1);
              const float sb = -vmin / (vmax - vmin) * (nbins - 1);
              for (int j = 0; j < arr.shape.y; ++j)
                for (int i = 0; i < arr.shape.x; ++i)
                {
                  int bin = static_cast<int>(sa * arr(i, j) + sb);
                  bin = bin < 0 ? 0 : (bin >= nbins ? nbins - 1 : bin);
                  d.series_y[bin] += 1.f;
                }
              return d;
            }});
  }

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

void compute_saturate_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    float hmin = p_in->min(node.cfg().cm_cpu);
    float hmax = p_in->max(node.cfg().cm_cpu);

    auto &c = node.meta_group().current();

    const glm::vec2 range = c.value<glm::vec2>(A_RANGE);
    const float      k     = c.value<float>(A_K_SMOOTHING);

    hmap::for_each_tile(
        {p_out, p_in},
        [&node, &hmin, &hmax, &range, &k](std::vector<hmap::Array *> p_arrays,
                              const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out = *pa_in;

          hmap::saturate(*pa_out,
                         range[0],
                         range[1],
                         hmin,
                         hmax,
                         k);
        },
        node.cfg().cm_cpu);

    // post-process
    post_process_heightmap(node, *p_out, p_in);
  }
}

} // namespace hesiod
