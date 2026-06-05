# The Node Settings Toolbar

Every node's settings panel has a toolbar of nine buttons across the top. The buttons are
icon-only — hover over one to see its tooltip. From left to right they let you re-run the
node, inspect it, manage temporary and saved configurations, and open its documentation.

![The node settings toolbar](node_settings_toolbar.png)

## Overview

| # | Button (tooltip)        | Icon                     | What it does                                          |
|---|-------------------------|--------------------------|------------------------------------------------------|
| 1 | **Force Update**        | circular refresh arrow   | Re-runs this node's computation.                     |
| 2 | **Node Information**    | "i" info symbol          | Opens the node information view.                      |
| 3 | **Backup State**        | bookmark                 | Snapshots the current settings into the backup slot. |
| 4 | **Revert State**        | U-turn-left arrow        | Swaps the current settings with the backup slot.     |
| 5 | **Load Preset**         | open document            | Loads settings from a file.                          |
| 6 | **Save Preset**         | floppy disk              | Saves the current settings to a file.                |
| 7 | **Reset Settings**      | circular restore arrow   | Restores the node's initial (default) settings.      |
| 8 | **Help!**               | question mark            | Opens the node's in-app documentation popup.         |
| 9 | **Online Documentation**| chain link               | Opens the node's page on the online documentation.   |

The rest of this page covers the buttons that have behaviour worth explaining in more
detail.

## Force Update

**Force Update** re-runs the node's computation on demand. Nodes normally recompute
automatically when an input or a parameter changes, so you rarely need this — it is handy
when you want to force a fresh evaluation (for example after changing a setting that feeds
randomness) without otherwise touching the graph.

## Backup and Revert State (swap mechanism)

The **Backup State** and **Revert State** buttons work together to provide a **manual
two-slot swap mechanism** for temporary state management during editing. The backup slot
lives in memory and is not saved with your project.

### How it works

1. **Backup State**: Saves the current configuration into the backup slot.
2. **Revert State**: Swaps the current configuration with the one stored in the backup
   slot.

This lets you toggle between two configurations, which is useful for quickly testing
variations.

### Example

1. Start with a node in **Config1**.
2. Click **Backup State** → Config1 is stored.
3. Modify the node → it becomes **Config2**.
4. Click **Revert State** → the node reverts to Config1, and Config2 is now stored.
5. Click **Revert State** again → the node reverts to Config2, and Config1 is now stored
   again.
6. This "swap" can continue back and forth.

### Continuing with new changes

- If you're happy with Config2, click **Backup State** to overwrite the backup slot with
  Config2.
- Modify the node to reach **Config3**.
- Now, clicking **Revert State** will swap between Config2 and Config3.

## Load and Save Presets

To **permanently store** or share node configurations, use:

- **Save Preset**: Saves the current configuration to a file.
- **Load Preset**: Loads a configuration from a file and applies it to the node.

Unlike the in-memory backup slot, presets are written to disk, so they are ideal for
managing multiple configuration profiles across sessions or between projects.

## Reset Settings

**Reset Settings** restores the node to its initial (default) settings. This is different
from **Revert State**: Revert swaps with the snapshot *you* took, whereas Reset discards
your changes entirely and returns the node to its defaults.

## Help and Online Documentation

The last two buttons both open documentation, but from different sources:

- **Help!** opens the node's documentation in an **in-app popup**, so you can read it
  without leaving Hesiod.
- **Online Documentation** opens the node's page in the **online documentation** in your
  web browser.
