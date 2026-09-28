/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <typeinfo>

#include "highmap/geometry/cloud.hpp"
#include "highmap/geometry/path.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/gui/widgets/viewers/viewer.hpp"
#include "hesiod/logger.hpp"

namespace hesiod
{

std::string helper_get_preferred_port_inout(const BaseNode                 &node,
                                            const std::type_info           &type,
                                            const std::vector<std::string> &exclude_names)
{
  std::string value;
  int         in_candidate = -1;

  auto is_excluded = [&](const std::string &name)
  {
    return std::find(exclude_names.begin(), exclude_names.end(), name) !=
           exclude_names.end();
  };

  for (int k = 0; k < node.get_nports(); ++k)
  {
    if (node.get_data_type(k) == type.name())
    {
      const std::string port_label = node.get_port_label(k);

      if (node.get_port_type(k) == gnode::PortType::OUT && !is_excluded(port_label))
      {
        value = port_label;
        break; // OUT has priority
      }
      else if (in_candidate == -1 && node.get_port_type(k) == gnode::PortType::IN &&
               !is_excluded(port_label))
      {
        in_candidate = k;
      }
    }
  }

  if (value.empty() && in_candidate != -1)
    value = node.get_port_label(in_candidate);

  return value;
}

std::string helper_get_preferred_port_label(
    const BaseNode                 &node,
    const std::type_info           &type,
    const std::vector<std::string> &preferred_labels)
{
  for (const std::string &label : preferred_labels)
  {
    for (int k = 0; k < node.get_nports(); ++k)
    {
      if (node.get_port_label(k) == label && node.get_data_type(k) == type.name())
        return label; // first preferred match
    }
  }

  return {};
}

void wild_guess_view_param(ViewerNodeParam &view_param,
                           const ViewerType & /* viewer_type */,
                           const BaseNode &node)
{
  std::string cat = node.get_category();

  for (auto &[key, value] : view_param.port_ids)
  {
    if (key == "elevation")
    {
      if (cat.find("Selector") != std::string::npos)
      {
        value = "input";
        continue;
      }

      value = helper_get_preferred_port_inout(node,
                                              typeid(hmap::VirtualArray),
                                              {"water_depth", "mask"});
    }

    if (key == "water_depth")
    {
      value = helper_get_preferred_port_label(node,
                                              typeid(hmap::VirtualArray),
                                              {"water_depth"});
    }

    if (key == "color")
    {
      if (cat.find("Selector") != std::string::npos)
      {
        value = "output";
        continue;
      }

      // try textures first
      value = helper_get_preferred_port_label(node,
                                              typeid(hmap::VirtualTexture),
                                              {"texture"});

      // fallback to mask if any
      if (value.empty())
        value = helper_get_preferred_port_label(node,
                                                typeid(hmap::VirtualArray),
                                                {"mask"});
    }

    if (key == "normal_map")
    {
      value = helper_get_preferred_port_label(node,
                                              typeid(hmap::VirtualTexture),
                                              {"normal map"});
    }

    if (key == "points")
    {
      value = helper_get_preferred_port_inout(node, typeid(hmap::Cloud), {});
    }

    if (key == "path")
    {
      value = helper_get_preferred_port_inout(node, typeid(hmap::Path), {});
    }
  }
}

} // namespace hesiod
