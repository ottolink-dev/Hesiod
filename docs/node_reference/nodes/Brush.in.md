## Painting resolution

New brushes store a 512 × 512 painting by default, independently of the graph
resolution. This uses 1 MiB of float data even in a 4096 × 4096 graph. The paint
canvas uses a separate 256 × 256 field; the node resamples the stored painting
to the graph resolution when computing its output.

Choose **New brush storage resolution** in **Settings → Node Editor** to change
the default for new Brush nodes (128–4096 pixels per side). Higher values use
more memory and produce larger project files. Existing paintings retain their
saved dimensions and values when loaded, duplicated, or saved again.
