# Export Formats

Hesiod has one export node per output kind. Pick the node whose **input data
type** matches what your graph produces (see
[Heightmaps & Virtual Arrays](../core_concepts/heightmaps.md) for the data model:
`VirtualArray` is scalar/height, `VirtualTexture` is RGBA colour — they are not
interchangeable).

| Node | Writes | Format(s) | Input port → type |
|------|--------|-----------|-------------------|
| `ExportHeightmap` | Grayscale heightmap | PNG 16-bit, PNG 8-bit, RAW 16-bit (Unity) | `input` → VirtualArray |
| `ExportTexture` | Colour image | PNG (8- or 16-bit) | `texture` → VirtualTexture |
| `ExportNormalMap` | Normal map of a heightmap | PNG (8- or 16-bit) | `input` → VirtualArray |
| `ExportAsset` | 3D mesh (+ optional texture/normal) | OBJ, FBX, GLB/GLTF, PLY, STL, COLLADA, 3DS, 3MF, X3D, STEP, Assimp | `elevation`, `mask` → VirtualArray; `texture`, `normal map details` → VirtualTexture |
| `ExportTiled` | Heightmap split into tiles | Per-tile images (selectable bit depth) | `input` → VirtualArray |
| `ExportCloud` | Point cloud | CSV | `input` → Cloud |
| `ExportCloudToPly` | Point cloud | PLY | `cloud` → Cloud (+ `point_data1..3` → vector\<float\>) |
| `ExportPointsToPly` | Point cloud | PLY | `x`/`y`/`z` + `point_data1..3` → vector\<float\> |
| `ExportPath` | Path / polyline | CSV | `input` → Path |

## Importing

Two nodes read external files back into a graph:

| Node | Reads | Output port → type |
|------|-------|--------------------|
| `ImportHeightmap` | Grayscale PNG (8-bit) | `output` → VirtualArray |
| `ImportTexture` | Image file | `texture` → VirtualTexture |

## Notes

- **Heightmap vs texture:** `ColorizeGradient`/`ColorizeSolid` convert a
  VirtualArray to a VirtualTexture, so a colourised result must go to
  `ExportTexture`, not `ExportHeightmap`. See
  [Texturing & Colorize](texturing.md).
- **`auto_export`:** every export node has an `auto_export` toggle and an `fname`
  parameter; with `auto_export` on, the node writes whenever it recomputes.
- **Whole-graph export:** to flatten and export a graph from the command line,
  see [Batch / Headless Mode](batch-headless.md). For the in-app bake workflow,
  see [Bake & Export](../user_manual/bake_and_export/bake_and_export.md).
