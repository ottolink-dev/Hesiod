/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/cli/batch_mode.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/port_catalog.hpp"

namespace hesiod
{

namespace
{

int failures = 0;

void expect_offerable(const PortCatalog &catalog,
                      const std::string &node_type,
                      const std::string &data_type,
                      gngui::PortType    wanted,
                      bool               expected)
{
  const bool got = catalog.is_offerable(node_type, data_type, wanted);
  if (got != expected)
  {
    Logger::log()->error("check-port-links: {} [{}, want {}]: offerable={} expected={}",
                         node_type,
                         data_type,
                         wanted == gngui::PortType::IN ? "IN" : "OUT",
                         got,
                         expected);
    failures++;
  }
}

} // namespace

int run_check_port_links()
{
  failures = 0;

  const PortCatalog catalog = PortCatalog::from_documentation();
  Logger::log()->info("check-port-links: catalog has {} node types", catalog.size());

  if (catalog.size() == 0)
  {
    Logger::log()->error("check-port-links: catalog is empty");
    return 1;
  }

  // --- pinned offer-rule cases

  // IslandChain's only VirtualArray port is its OUTPUT, so dragging a
  // VirtualArray from an output (wanting an input) must NOT offer it.
  // This is the case that aborted the application.
  expect_offerable(catalog, "IslandChain", "VirtualArray", gngui::PortType::IN, false);

  // Dragging backwards from an input (wanting an output) must offer it.
  expect_offerable(catalog, "IslandChain", "VirtualArray", gngui::PortType::OUT, true);

  // Its Path input is offerable when a Path is dragged from an output.
  expect_offerable(catalog, "IslandChain", "Path", gngui::PortType::IN, true);

  // Ordinary filters accept a VirtualArray input.
  expect_offerable(catalog, "Laplace", "VirtualArray", gngui::PortType::IN, true);
  expect_offerable(catalog, "Bump", "VirtualArray", gngui::PortType::IN, true);

  // Incompatible type is never offered.
  expect_offerable(catalog, "Laplace", "VirtualTexture", gngui::PortType::IN, false);

  // Unknown node type fails OPEN (never hide a real node if docs drift).
  expect_offerable(catalog, "NoSuchNodeType", "VirtualArray", gngui::PortType::IN, true);

  Logger::log()->info("check-port-links: {} failure(s)", failures);
  return failures ? 1 : 0;
}

} // namespace hesiod
