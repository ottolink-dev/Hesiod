/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/cli/batch_mode.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/graph/graph_config.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/node_factory.hpp"
#include "hesiod/model/nodes/port_catalog.hpp"

#include <cctype>
#include <set>

namespace hesiod
{

namespace
{

int failures = 0;
int conventional_rule_changed_outcome = 0;

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

void expect_selected(const std::string &node_type,
                     const std::string &data_type,
                     gngui::PortType    wanted,
                     const std::string &expected_port)
{
  auto  config = std::make_shared<hesiod::GraphConfig>();
  auto  p_node = hesiod::node_factory(node_type, config);
  auto *p_base = dynamic_cast<hesiod::BaseNode *>(p_node.get());

  if (!p_base)
  {
    Logger::log()->error("check-port-links: could not build node '{}'", node_type);
    failures++;
    return;
  }

  const std::optional<std::string> got = hesiod::select_port(*p_base, data_type, wanted);
  const std::string got_str = got ? *got : std::string("<none>");

  if (got_str != expected_port)
  {
    Logger::log()->error("check-port-links: {} [{}, want {}]: selected '{}' expected '{}'",
                         node_type,
                         data_type,
                         wanted == gngui::PortType::IN ? "IN" : "OUT",
                         got_str,
                         expected_port);
    failures++;
  }
}

// Sweep every node type against every data type in both directions and assert
// the invariants that make the feature safe:
//   Safety    - if a type is offered, the live node really has a port of the
//               required direction, so new_link can never throw.
//   Agreement - the documentation catalog and the live node agree, so a docs
//               drift becomes a failing check instead of a mis-filtered menu.
//   Oracle    - select_port's chosen port matches an independent
//               recomputation of the selection rule (conventional name wins,
//               else first declared) against the live node's declaration
//               order, so a silent regression to plain first-declared cannot
//               slip through undetected.
void sweep_all_node_types(const PortCatalog &catalog)
{
  // collect every data type that appears anywhere in the catalog
  std::set<std::string> data_types;
  for (const auto &[node_type, category] : hesiod::get_node_inventory())
    if (const std::vector<PortInfo> *infos = catalog.find(node_type))
      for (const PortInfo &p : *infos)
        data_types.insert(p.data_type);

  Logger::log()->info("check-port-links: sweeping {} node types x {} data types",
                      hesiod::get_node_inventory().size(),
                      data_types.size());

  auto config = std::make_shared<hesiod::GraphConfig>();

  for (const auto &[node_type, category] : hesiod::get_node_inventory())
  {
    auto  p_node = hesiod::node_factory(node_type, config);
    auto *p_base = dynamic_cast<hesiod::BaseNode *>(p_node.get());

    if (!p_base)
    {
      Logger::log()->error("check-port-links: could not build node '{}'", node_type);
      failures++;
      continue;
    }

    for (const std::string &data_type : data_types)
      for (gngui::PortType wanted : {gngui::PortType::IN, gngui::PortType::OUT})
      {
        const bool offered = catalog.is_offerable(node_type, data_type, wanted);
        const std::optional<std::string> selected = hesiod::select_port(*p_base,
                                                                        data_type,
                                                                        wanted);

        // Agreement: catalog and live node must say the same thing.
        if (offered != selected.has_value())
        {
          Logger::log()->error(
              "check-port-links: {} [{}, want {}]: catalog offered={} but live "
              "node selected={} (documentation drift?)",
              node_type,
              data_type,
              wanted == gngui::PortType::IN ? "IN" : "OUT",
              offered,
              selected.has_value());
          failures++;
          continue;
        }

        // Safety: a selected port must really have the required direction.
        if (selected)
        {
          const int index = p_base->get_port_index(*selected);
          if (p_base->get_port_type(index) != wanted)
          {
            Logger::log()->error(
                "check-port-links: {}: selected port '{}' has the wrong direction",
                node_type,
                *selected);
            failures++;
          }
        }

        // Oracle: independently recompute the expected selection directly
        // from the live node's declaration order and compare.
        std::optional<std::string> expected;
        std::optional<std::string> first_declared;
        for (int k = 0; k < p_base->get_nports(); ++k)
        {
          if (p_base->get_port_type(k) != wanted) continue;
          if (map_type_name(p_base->get_data_type(k)) != data_type) continue;

          const std::string label = p_base->get_port_label(k);
          if (!first_declared) first_declared = label;

          std::string lower;
          for (char c : label)
            lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
          const bool conventional = (wanted == gngui::PortType::IN)
                                        ? (lower == "input" || lower == "in")
                                        : (lower == "output" || lower == "out");
          if (conventional)
          {
            expected = label;
            break;
          }
          if (!expected) expected = label;
        }

        if (expected != selected)
        {
          Logger::log()->error(
              "check-port-links: {} [{}, want {}]: oracle expected='{}' got='{}'",
              node_type,
              data_type,
              wanted == gngui::PortType::IN ? "IN" : "OUT",
              expected ? *expected : std::string("<none>"),
              selected ? *selected : std::string("<none>"));
          failures++;
        }

        // Informational: did the conventional-name preference actually
        // change the outcome versus plain first-declared?
        if (expected && first_declared && *expected != *first_declared)
          conventional_rule_changed_outcome++;
      }
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

  // --- pinned port-selection cases (live node, true declaration order)

  // Conventional name wins: Laplace declares an "input" port.
  expect_selected("Laplace", "VirtualArray", gngui::PortType::IN, "input");

  // No conventional name: Bump declares dx, dy, control, envelope -> first
  // declared wins. NOTE this is "dx" only because selection reads the LIVE
  // node; the documentation's alphabetical key order would have given
  // "control", which is why the catalog must never be used for selection.
  expect_selected("Bump", "VirtualArray", gngui::PortType::IN, "dx");

  // Backwards drag: wanting an OUTPUT of type VirtualArray.
  expect_selected("IslandChain", "VirtualArray", gngui::PortType::OUT, "out");

  // Forwards drag onto IslandChain has no VirtualArray input at all.
  expect_selected("IslandChain", "VirtualArray", gngui::PortType::IN, "<none>");

  sweep_all_node_types(catalog);

  Logger::log()->info("check-port-links: {} triples where the conventional-name rule "
                      "changed the outcome",
                      conventional_rule_changed_outcome);

  Logger::log()->info("check-port-links: {} failure(s)", failures);
  return failures ? 1 : 0;
}

} // namespace hesiod
