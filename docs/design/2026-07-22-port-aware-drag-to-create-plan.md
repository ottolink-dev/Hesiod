# Port-Aware Drag-to-Create — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** When you drag from a node port onto empty canvas, the creation menu lists only node types that can actually connect — and the auto-connect afterwards is deterministic and cannot abort the application.

**Architecture:** A `PortCatalog` built from the already-loaded node documentation answers *"can this node type connect?"* before any node exists (used to filter the menu). A separate `select_port` answers *"which port?"* against the **live** node after creation, because only the live node preserves true declaration order. `on_connection_dropped` becomes thin Qt glue: resolve the dragged port, filter, create, connect — with operands ordered so `gnode::Graph::new_link`'s direction checks are unreachable.

**Tech Stack:** C++20, Qt6, nlohmann::json, GNodeGUI (`gngui::PortType`), nix devshell build.

**Spec:** `docs/design/2026-07-22-port-aware-drag-to-create-design.md`

## Global Constraints

- **Hesiod-side only.** Do NOT modify `external/GNodeGUI`, `external/GNode`, `external/Meta`, or `external/HighMap`. The menu filter is achieved by setting a filtered inventory on the existing public `GraphViewer::set_node_inventory()` and restoring it — no submodule change is needed.
- **Staging discipline:** `git add` ONLY the files each task names. NEVER `git add -A`/`.`. NEVER stage `external/*` (the dirty `GNode`/`GNodeGUI`/`HighMap` submodules are the user's checkout deltas) or untracked screenshots/docs. Run `git status` before every commit.
- No `Co-Authored-By` lines in commit messages.
- **New `.cpp` files need a CMake reconfigure.** The Hesiod source glob lacks `CONFIGURE_DEPENDS`, so after adding a new source file run `nix develop ~/quixote#cpp-qt-desktop -c bash -c 'cd /home/barrulus/dev/Hesiod && cmake -B build'` once before building.
- **BUILD-HESIOD** (run via Bash `run_in_background: true`; NEVER unbounded `-j` — it OOMs):
  ```bash
  nix develop ~/quixote#cpp-qt-desktop -c bash -c 'cd /home/barrulus/dev/Hesiod && cmake --build build -j4 --target hesiod 2>&1 | tail -20; echo BUILD_EXIT=${PIPESTATUS[0]}'
  ```
  Success: `Linking CXX executable ... bin/hesiod`, `BUILD_EXIT=0`.
- **RUN-CHECK** (headless; the binary SIGABRTs without the offscreen platform):
  ```bash
  nix develop ~/quixote#cpp-qt-desktop -c bash -c 'cd /home/barrulus/dev/Hesiod/Hesiod/data && QT_QPA_PLATFORM=offscreen ../../build/bin/hesiod --check-port-links 2>&1 | tail -25; echo CHECK_EXIT=${PIPESTATUS[0]}'
  ```
- There is **no C++ unit-test harness** in this repo (`tests/` is Python and unwired from CMake). The `--check-port-links` subcommand IS the test vehicle, following the existing `--parity-dump` / `--compat-check` pattern. "Write the failing test" means "write the check assertion first and watch it fail".
- GUI behaviour is **user-verified**. Never claim a GUI outcome yourself.

## File Structure

| File | Responsibility |
|---|---|
| `Hesiod/include/hesiod/model/nodes/port_catalog.hpp` (new) | `PortInfo`, `PortCatalog`, `select_port` declarations. No Qt. |
| `Hesiod/src/model/nodes/port_catalog.cpp` (new) | Catalog build from documentation; `is_offerable`; `select_port`. |
| `Hesiod/src/cli/check_port_links.cpp` (new) | `run_check_port_links()` — the verification sweep. |
| `Hesiod/include/hesiod/cli/batch_mode.hpp` (modify) | Declare `run_check_port_links()`. |
| `Hesiod/src/cli/batch_mode.cpp` (modify) | `--check-port-links` flag + dispatch. |
| `Hesiod/src/gui/widgets/graph_node_widget.cpp` (modify) | Rewrite `on_connection_dropped`. |

---

### Task 1: PortCatalog and the offer rule

**Files:**
- Create: `Hesiod/include/hesiod/model/nodes/port_catalog.hpp`
- Create: `Hesiod/src/model/nodes/port_catalog.cpp`
- Create: `Hesiod/src/cli/check_port_links.cpp`
- Modify: `Hesiod/include/hesiod/cli/batch_mode.hpp`
- Modify: `Hesiod/src/cli/batch_mode.cpp`

**Interfaces:**
- Consumes: `HSD_CTX.node_documentation` (`nlohmann::json`, loaded by `AppContext::load_node_documentation`); `gngui::PortType { IN, OUT }`.
- Produces (Tasks 2-4 rely on these exact names):
  - `struct hesiod::PortInfo { std::string name; std::string data_type; gngui::PortType direction; };`
  - `hesiod::PortCatalog::from_documentation() -> PortCatalog`
  - `hesiod::PortCatalog::is_offerable(const std::string &node_type, const std::string &data_type, gngui::PortType wanted_direction) const -> bool`
  - `hesiod::PortCatalog::find(const std::string &node_type) const -> const std::vector<PortInfo>*`
  - `hesiod::PortCatalog::size() const -> std::size_t`
  - `int hesiod::run_check_port_links()`

- [ ] **Step 1: Write the failing check** — create `Hesiod/src/cli/check_port_links.cpp`:

```cpp
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
```

- [ ] **Step 2: Declare the entry point.** In `Hesiod/include/hesiod/cli/batch_mode.hpp`, next to the existing `run_compat_check` declaration, add:

```cpp
/**
 * @brief Verify the drag-to-create port rules (offer rule, port selection,
 * and docs/live agreement). Returns 0 when every invariant holds.
 */
int run_check_port_links();
```

- [ ] **Step 3: Wire the CLI flag.** In `Hesiod/src/cli/batch_mode.cpp`, alongside the existing `compat_check` flag declaration add:

```cpp
  args::Flag check_port_links(group,
                              "check-port-links",
                              "verify drag-to-create port rules",
                              {"check-port-links"});
```

and add a dispatch branch next to the `compat_check` branch:

```cpp
    else if (check_port_links)
    {
      return hesiod::run_check_port_links();
    }
```

- [ ] **Step 4: Reconfigure and build — expect FAILURE.**

Run the CMake reconfigure (new source files), then BUILD-HESIOD.
Expected: compile error — `port_catalog.hpp` does not exist / `PortCatalog` undeclared. That is the RED state: the check names an interface that has not been built yet.

- [ ] **Step 5: Write the header** — create `Hesiod/include/hesiod/model/nodes/port_catalog.hpp`:

```cpp
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
```

- [ ] **Step 6: Implement the catalog** — create `Hesiod/src/model/nodes/port_catalog.cpp` (leave `select_port` for Task 2):

```cpp
/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/model/nodes/port_catalog.hpp"

#include "hesiod/app/app_context.hpp"
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
```

- [ ] **Step 7: Build and run the check — expect PASS.**

Reconfigure if needed, BUILD-HESIOD (expect `BUILD_EXIT=0`), then RUN-CHECK.
Expected output: `check-port-links: catalog has 302 node types`, `check-port-links: 0 failure(s)`, `CHECK_EXIT=0`.

If a pinned case fails, the catalog build is wrong — fix it, do not weaken the assertion.

- [ ] **Step 8: Commit**

```bash
cd /home/barrulus/dev/Hesiod
git status --short
git add Hesiod/include/hesiod/model/nodes/port_catalog.hpp \
        Hesiod/src/model/nodes/port_catalog.cpp \
        Hesiod/src/cli/check_port_links.cpp \
        Hesiod/include/hesiod/cli/batch_mode.hpp \
        Hesiod/src/cli/batch_mode.cpp
git commit -m "feat(gui): port catalog and offer rule for drag-to-create, with --check-port-links"
```

---

### Task 2: Port selection against the live node

**Files:**
- Modify: `Hesiod/src/model/nodes/port_catalog.cpp` (add `select_port`)
- Modify: `Hesiod/src/cli/check_port_links.cpp` (add selection assertions)

**Interfaces:**
- Consumes: Task 1's `PortCatalog`, `PortInfo`, `select_port` declaration; `BaseNode::get_nports() const`, `get_port_type(int) const -> gngui::PortType`, `get_data_type(int) const`, `get_port_label(int) const` (all const).
- Produces: a working `hesiod::select_port(const BaseNode&, const std::string&, gngui::PortType) -> std::optional<std::string>`, used by Tasks 3 and 4.

- [ ] **Step 1: Write the failing assertions.** In `Hesiod/src/cli/check_port_links.cpp`, add these includes at the top:

```cpp
#include "hesiod/model/graph_config.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/node_factory.hpp"
```

add this helper inside the anonymous namespace (after `expect_offerable`):

```cpp
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
```

and add these calls in `run_check_port_links()` after the offer-rule block:

```cpp
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
```

- [ ] **Step 2: Build — expect FAILURE.**

Run BUILD-HESIOD. Expected: link error — `hesiod::select_port` declared but not defined. That is RED.

- [ ] **Step 3: Implement `select_port`.** Append to `Hesiod/src/model/nodes/port_catalog.cpp` (add `#include <cctype>` and `#include "hesiod/model/nodes/base_node.hpp"` at the top of the file):

```cpp
namespace
{

bool is_conventional_name(const std::string &name, gngui::PortType direction)
{
  std::string lower;
  lower.reserve(name.size());
  for (char c : name)
    lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

  if (direction == gngui::PortType::IN)
    return lower == "input" || lower == "in";

  return lower == "output" || lower == "out";
}

} // namespace

std::optional<std::string> select_port(const BaseNode    &node,
                                       const std::string &data_type,
                                       gngui::PortType    wanted_direction)
{
  std::optional<std::string> first_match;

  for (int k = 0; k < node.get_nports(); ++k)
  {
    if (node.get_port_type(k) != wanted_direction)
      continue;

    if (node.get_data_type(k) != data_type)
      continue;

    const std::string label = node.get_port_label(k);

    // a conventionally-named port wins immediately
    if (is_conventional_name(label, wanted_direction))
      return label;

    // otherwise remember the first declared match
    if (!first_match)
      first_match = label;
  }

  return first_match;
}
```

Place this inside `namespace hesiod { ... }`, after `is_offerable`.

- [ ] **Step 4: Build and run the check — expect PASS.**

BUILD-HESIOD (`BUILD_EXIT=0`), then RUN-CHECK.
Expected: `check-port-links: 0 failure(s)`, `CHECK_EXIT=0`.

If `Bump` selects `control` instead of `dx`, selection is reading the catalog rather than the live node — fix the implementation, not the assertion.

- [ ] **Step 5: Commit**

```bash
cd /home/barrulus/dev/Hesiod
git status --short
git add Hesiod/src/model/nodes/port_catalog.cpp Hesiod/src/cli/check_port_links.cpp
git commit -m "feat(gui): select_port picks the connect target from the live node's declaration order"
```

---

### Task 3: Filter the menu and connect safely

**Files:**
- Modify: `Hesiod/src/gui/widgets/graph_node_widget.cpp` (`on_connection_dropped`)

**Interfaces:**
- Consumes: Task 1 `PortCatalog::from_documentation`, `is_offerable`; Task 2 `select_port`; `get_node_inventory()` (`Hesiod/include/hesiod/model/nodes/node_factory.hpp`, returns `std::map<std::string,std::string>` of node type → category); `GraphViewer::set_node_inventory(const std::map<std::string,std::string>&)`; `GraphViewer::execute_new_node_context_menu()` (blocking — it ends in `menu->exec()`).
- Produces: nothing consumed by later tasks.

- [ ] **Step 1: Replace `on_connection_dropped`.** In `Hesiod/src/gui/widgets/graph_node_widget.cpp`, replace the whole existing `void GraphNodeWidget::on_connection_dropped(...)` function body with:

```cpp
void GraphNodeWidget::on_connection_dropped(const std::string &node_id,
                                            const std::string &port_id,
                                            QPointF /*scene_pos*/)
{
  Logger::log()->trace("GraphNodeWidget::on_connection_dropped: {}/{}", node_id, port_id);

  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  BaseNode *p_node_from = gno->get_node_ref_by_id<BaseNode>(node_id);
  if (!p_node_from)
    return;

  // --- what was dragged, and what would we need on the other end?

  const int         from_index = p_node_from->get_port_index(port_id);
  const std::string dragged_type = p_node_from->get_data_type(from_index);
  const gngui::PortType dragged_dir = p_node_from->get_port_type(from_index);
  const gngui::PortType wanted_dir = (dragged_dir == gngui::PortType::OUT)
                                         ? gngui::PortType::IN
                                         : gngui::PortType::OUT;

  // --- offer only node types that can actually connect
  //
  // The menu is built by GraphViewer from its node inventory, so filtering is
  // done by swapping the inventory around the (blocking) menu call and putting
  // the full one back afterwards.

  const std::map<std::string, std::string> full_inventory = get_node_inventory();
  const PortCatalog                        catalog = PortCatalog::from_documentation();

  std::map<std::string, std::string> filtered;
  for (const auto &[node_type, category] : full_inventory)
    if (catalog.is_offerable(node_type, dragged_type, wanted_dir))
      filtered[node_type] = category;

  // if nothing accepts this type, fall back to the full list rather than
  // opening an empty menu
  const bool use_filtered = !filtered.empty();

  if (use_filtered)
    this->set_node_inventory(filtered);

  const bool created = this->execute_new_node_context_menu();

  if (use_filtered)
    this->set_node_inventory(full_inventory);

  if (!created)
    return;

  // --- connect the node that was just created

  const std::string node_to = this->last_node_created_id;
  BaseNode         *p_node_to = gno->get_node_ref_by_id<BaseNode>(node_to);

  if (!p_node_to)
  {
    Logger::log()->trace("GraphNodeWidget::on_connection_dropped: p_node_to is nullptr");
    return;
  }

  const std::optional<std::string> port_to = select_port(*p_node_to,
                                                         dragged_type,
                                                         wanted_dir);

  if (!port_to)
  {
    Logger::log()->trace(
        "GraphNodeWidget::on_connection_dropped: node '{}' has no {} port of type {}, "
        "leaving it unconnected",
        node_to,
        wanted_dir == gngui::PortType::IN ? "input" : "output",
        dragged_type);
    return;
  }

  // order the operands so that 'from' is always the OUTPUT side
  const bool dragged_is_output = (dragged_dir == gngui::PortType::OUT);

  const std::string id_out = dragged_is_output ? node_id : node_to;
  const std::string port_out = dragged_is_output ? port_id : *port_to;
  const std::string id_in = dragged_is_output ? node_to : node_id;
  const std::string port_in = dragged_is_output ? *port_to : port_id;

  // model first: only draw the GUI link if the model accepted it
  try
  {
    gno->new_link(id_out, port_out, id_in, port_in);
  }
  catch (const std::exception &e)
  {
    Logger::log()->error("GraphNodeWidget::on_connection_dropped: link refused: {}",
                         e.what());
    return;
  }

  this->add_link(id_out, port_out, id_in, port_in);
  gno->update(node_to);
}
```

- [ ] **Step 2: Add the includes.** At the top of `Hesiod/src/gui/widgets/graph_node_widget.cpp`, add (if not already present):

```cpp
#include <optional>

#include "hesiod/model/nodes/port_catalog.hpp"
```

`node_factory.hpp` (for `get_node_inventory`) is already included — this file calls `get_node_inventory()` in its constructor.

- [ ] **Step 3: Build**

Run BUILD-HESIOD. Expected `BUILD_EXIT=0`.

- [ ] **Step 4: Re-run the check (guard against regressions)**

RUN-CHECK. Expected `check-port-links: 0 failure(s)`, `CHECK_EXIT=0` — this task must not change catalog or selection behaviour.

- [ ] **Step 5: USER GUI VERIFICATION (pause here).**

Ask barrulus to launch `build/bin/hesiod` and confirm:
1. Drag from **Bump**'s `output` onto empty canvas → the menu no longer lists `IslandChain` (and other nodes with no `VirtualArray` input). Creating e.g. `Laplace` connects to its `input`. **No crash** — this is the reported abort.
2. Drag from a node's **input** backwards onto canvas → menu lists nodes that produce that type; creating one connects `new.out → your input`. **No crash.**
3. Drag a `Path` output (e.g. from a Path node) → the menu offers Path consumers.
4. Plain right-click on empty canvas (no drag) still shows the **full** node list.
5. Creating a node whose menu was filtered and then cancelling the menu leaves the graph unchanged.

Do not proceed until the user reports.

- [ ] **Step 6: Commit**

```bash
cd /home/barrulus/dev/Hesiod
git status --short
git add Hesiod/src/gui/widgets/graph_node_widget.cpp
git commit -m "fix(gui): port-aware drag-to-create — filter the menu and connect safely in both directions"
```

---

### Task 4: Full sweep invariants

**Files:**
- Modify: `Hesiod/src/cli/check_port_links.cpp`

**Interfaces:**
- Consumes: Tasks 1-2. No new public interface.

- [ ] **Step 1: Add the sweep.** In `Hesiod/src/cli/check_port_links.cpp`, add this function inside the anonymous namespace:

```cpp
// Sweep every node type against every data type in both directions and assert
// the two invariants that make the feature safe:
//   Safety    - if a type is offered, the live node really has a port of the
//               required direction, so new_link can never throw.
//   Agreement - the documentation catalog and the live node agree, so a docs
//               drift becomes a failing check instead of a mis-filtered menu.
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
      }
  }
}
```

Add these includes at the top of the file:

```cpp
#include <set>
```

and call it from `run_check_port_links()` just before the final log line:

```cpp
  sweep_all_node_types(catalog);
```

- [ ] **Step 2: Build and run — expect PASS.**

BUILD-HESIOD, then RUN-CHECK.
Expected: `check-port-links: sweeping 302 node types x N data types`, then `check-port-links: 0 failure(s)`, `CHECK_EXIT=0`.

If the **Agreement** invariant fails, the documentation is out of date with the code for that node type — report it rather than relaxing the check; regenerating the node documentation is the fix.

- [ ] **Step 3: Commit**

```bash
cd /home/barrulus/dev/Hesiod
git status --short
git add Hesiod/src/cli/check_port_links.cpp
git commit -m "test(gui): sweep all node types for drag-to-create safety and docs/live agreement"
```

---

## Self-Review notes

- **Spec coverage:** §4 components → Tasks 1 (catalog, `is_offerable`), 2 (`select_port`), 3 (menu hook + thin slot). §5 behaviour contract → Task 3 (offer rule, symmetry table, empty-set fallback, no-match) with the selection rule pinned in Task 2. §6 error handling → Task 3 (operand ordering, try/catch, model-link-before-GUI-link, cancelled menu). §7 verification → Tasks 1, 2 and 4 (pinned cases plus the two sweep invariants); manual GUI checks are Task 3 Step 5.
- **Deviation from the spec, flagged:** the spec described the menu hook as "`execute_new_node_context_menu()` accepts an optional predicate". That function belongs to `GraphViewer` in the **GNodeGUI submodule**, so adding a parameter would drag a third repository into the change. Task 3 instead swaps the viewer's node inventory around the blocking menu call and restores it — same behaviour, Hesiod-only. Non-drag creation paths are untouched because they never swap the inventory.
- **Known limitation, accepted:** dropping onto a multi-input node connects the first declared compatible input (`Bump` → `dx`); the user rewires when a different input was intended.
