/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/operator.hpp"
#include "highmap/virtual_array/virtual_array.hpp"

#include "meta/core/data_provider.hpp"
#include "meta/metadata/keys.hpp"
#include "meta_qt/widgets/range_bar.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

void setup_histogram_for_range_attribute(BaseNode          &node,
                                         const std::string &attribute_key,
                                         const std::string &port_id)
{
  Logger::log()->trace("setup_histogram_for_range_slider: node {}", node.get_label());

  auto provider = [&node, port_id]() -> meta::Any
  {
    hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(port_id);
    if (!p_in)
      return {};
    float vmin = p_in->min(node.cfg().cm_cpu);
    float vmax = p_in->max(node.cfg().cm_cpu);
    if (vmin == vmax)
      return {};
    const int   nbins = 256;
    hmap::Array arr = p_in->to_array({256, 256}, node.cfg().cm_cpu);
    meta::qt::HistogramData d;
    d.x = hmap::linspace(vmin, vmax, nbins, false);
    d.y.assign(nbins, 0.f);
    const float sa = 1.f / (vmax - vmin) * (nbins - 1);
    const float sb = -vmin / (vmax - vmin) * (nbins - 1);
    for (int j = 0; j < arr.shape.y; ++j)
      for (int i = 0; i < arr.shape.x; ++i)
      {
        int bin = static_cast<int>(sa * arr(i, j) + sb);
        bin = bin < 0 ? 0 : (bin >= nbins ? nbins - 1 : bin);
        d.y[bin] += 1.f;
      }
    return d;
  };

  auto &c = node.meta_group().current();
  auto *p = c.find(attribute_key);
  if (!p)
  {
    Logger::log()->error("setup_histogram_for_range_attribute: meta key '{}' not found",
                         attribute_key);
    return;
  }
  p->metadata().try_add(std::string(meta::keys::ui::data_provider),
                        meta::DataProvider(provider));
}

} // namespace hesiod
