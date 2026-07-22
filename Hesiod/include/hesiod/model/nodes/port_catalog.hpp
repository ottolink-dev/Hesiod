/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "gnodegui/node_proxy.hpp" // gngui::PortType

namespace hesiod
{

class BaseNode;

/// One port of a node type, as described by the node documentation.
struct PortInfo
{
  std::string     name;
  std::string     data_type;
  gngui::PortType direction;
};

/**
 * @brief Port metadata for every node TYPE, read from the node documentation
 * loaded at startup.
 *
 * This answers "can this node type connect?" BEFORE any node exists, which is
 * what the creation menu needs. It must NOT be used to choose WHICH port to
 * connect: the documentation stores ports in a JSON object whose key order is
 * alphabetical, not declaration order. Use select_port() on the live node for
 * that.
 */
class PortCatalog
{
public:
  /// Build from the documentation held by the application context.
  static PortCatalog from_documentation();

  /**
   * @brief Does this node type have a port of the wanted direction and data
   * type? An unknown node type returns true (fail-open), so a documentation
   * gap can never hide a real node from the menu.
   */
  bool is_offerable(const std::string &node_type,
                    const std::string &data_type,
                    gngui::PortType    wanted_direction) const;

  /// Ports of a node type, or nullptr when the type is unknown.
  const std::vector<PortInfo> *find(const std::string &node_type) const;

  std::size_t size() const { return this->ports.size(); }

private:
  std::map<std::string, std::vector<PortInfo>> ports;
};

/**
 * @brief Which port of a LIVE node should be connected?
 *
 * Uses the node's true declaration order. Prefers a conventionally-named port
 * for the direction sought ("input"/"in", or "output"/"out", case-insensitive),
 * otherwise the first declared match. Returns nothing when no port matches.
 */
std::optional<std::string> select_port(const BaseNode    &node,
                                       const std::string &data_type,
                                       gngui::PortType    wanted_direction);

} // namespace hesiod
