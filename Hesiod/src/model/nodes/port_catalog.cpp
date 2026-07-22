/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/model/nodes/port_catalog.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/logger.hpp"

namespace hesiod
{

PortCatalog PortCatalog::from_documentation()
{
  PortCatalog catalog;

  const nlohmann::json &docs = HSD_CTX.node_documentation;

  for (auto &[node_type, entry] : docs.items())
  {
    if (!entry.is_object() || !entry.contains("ports") || !entry["ports"].is_object())
      continue;

    std::vector<PortInfo> infos;

    for (auto &[port_name, port] : entry["ports"].items())
    {
      if (!port.is_object() || !port.contains("data_type") || !port.contains("type"))
        continue;

      PortInfo info;
      info.name = port_name;
      info.data_type = port["data_type"].get<std::string>();
      info.direction = (port["type"].get<std::string>() == "input") ? gngui::PortType::IN
                                                                   : gngui::PortType::OUT;
      infos.push_back(std::move(info));
    }

    catalog.ports[node_type] = std::move(infos);
  }

  return catalog;
}

const std::vector<PortInfo> *PortCatalog::find(const std::string &node_type) const
{
  auto it = this->ports.find(node_type);
  return (it == this->ports.end()) ? nullptr : &it->second;
}

bool PortCatalog::is_offerable(const std::string &node_type,
                               const std::string &data_type,
                               gngui::PortType    wanted_direction) const
{
  const std::vector<PortInfo> *infos = this->find(node_type);

  // fail-open: an undocumented node type stays visible in the menu
  if (!infos)
    return true;

  for (const PortInfo &p : *infos)
    if (p.direction == wanted_direction && p.data_type == data_type)
      return true;

  return false;
}

} // namespace hesiod
