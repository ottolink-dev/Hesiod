# Batch / Headless Mode

Hesiod can run without the GUI to compute and export a graph from the command
line — useful for automation, large renders, and CI.

!!! note "Full guide coming soon"
    A complete automation & batch reference is in progress. This page will be
    expanded to cover the CLI surface, export configuration, and scripting
    patterns. For now: a graph runs headlessly with `hesiod --batch <graph>.hsd`
    and will flatten and export **only if the graph defines an export path**.

See also: [Export Formats](export-formats.md) ·
[Bake & Export](../user_manual/bake_and_export/bake_and_export.md).
