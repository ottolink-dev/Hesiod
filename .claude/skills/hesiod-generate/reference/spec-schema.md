# Hesiod spec format — field reference

A spec is a JSON object with four top-level keys: `config`, `nodes`, `links`, and `export`. All
keys are optional except that a spec without `export` produces no output files.

---

## Top-level keys

### `config`

Controls the canvas size and tiling behaviour for the render.

| Field | JSON type | Default | Meaning |
|---|---|---|---|
| `shape` | `[w, h]` | `[1024, 1024]` | Output dimensions in pixels |
| `tiling` | `[x, y]` | `[1, 1]` | Number of tiles along each axis |
| `overlap` | number (float) | `0.0` | Blending margin ratio (0–1); use 0.25 for tiled large maps |

`config` may be omitted entirely; the defaults above apply. Individual fields may also be
overridden at run time with `--shape`, `--tiling`, `--overlap` flags on `hsd make` or `hsd run`
without editing the spec file.

```json
"config": {"shape": [1024, 1024], "tiling": [1, 1], "overlap": 0.0}
```

---

### `nodes`

An array of node objects. Each node must have at minimum `id` and `type`.

| Field | JSON type | Required | Meaning |
|---|---|---|---|
| `id` | string | yes | Arbitrary identifier; scopes link endpoints as `"id.port"` |
| `type` | string | yes | Hesiod node type name (exact, case-sensitive) |
| `params` | object | no | Param overrides; omitted params use Hesiod's defaults |

```json
"nodes": [
  {"id": "noise", "type": "NoiseFbm", "params": {"kw": [4, 4], "seed": 1}},
  {"id": "ero",   "type": "HydraulicParticle"},
  {"id": "exp",   "type": "ExportHeightmap"}
]
```

`id` values are free-form strings. They do not need to match the node type.

---

### `links`

An array of two-element arrays connecting ports. Each element is a string in `"id.port"` form.

```json
"links": [
  ["noise.output", "ero.input"],
  ["ero.output",   "exp.input"]
]
```

Each entry connects the named output port of one node to the named input port of another. Port
names must match exactly what `hsd nodes --show TYPE` reports — they are not guessable.

Links must respect data-type compatibility. See the [hard port-datatype rule](#port-datatype-rule)
below and in `SKILL.md §4`.

---

### `export`

An array of export entries. Each entry names a node, a port on that node, and a file path to
write. The toolkit writes:

- `<path>.png` — 16-bit grayscale heightmap (for `VirtualArray` exports) or RGBA texture (for
  `VirtualTexture` exports). *The `.png` extension is appended to whatever `path` you provide if
  it is not already present.*
- `<path>_preview.png` — TERRAIN colourmap + hillshade preview image.

| Field | JSON type | Meaning |
|---|---|---|
| `node` | string | The `id` of the node to export from |
| `port` | string | The port name on that node |
| `path` | string | Output file path (relative or absolute) |

```json
"export": [
  {"node": "ero", "port": "output", "path": "terrain.png"}
]
```

A spec without an `export` key (or with an empty array) is valid and compiles to a `.hsd` that
renders, but produces no output files.

---

## `params` — supported scalar types

Only list params you want to override. Omitted params fall back to Hesiod's node defaults (the
loader is tolerant of missing params).

The following param types can be set with direct JSON values:

| Catalog type string | JSON form | Example |
|---|---|---|
| `Float` | number | `0.5` |
| `Integer` | integer | `4` |
| `Bool` | boolean | `true` |
| `Random seed number` | integer | `42` |
| `Wavenumber` | `[x, y]` | `[4, 4]` |
| `Vec2Float` | `[x, y]` | `[0.5, 0.5]` |
| `Value range` | `[lo, hi]` | `[0.0, 1.0]` |
| `Choice` | string | `"turbulence"` |
| `Enumeration` | string | `"maximum"` |
| `String` | string | `"my_label"` |
| `Color` | `[r, g, b, a]` | `[0.2, 0.4, 0.8, 1.0]` |

**Enumeration and Choice params accept a plain string.** The toolkit resolves it to the correct
integer at compile time. Run `hsd nodes --show TYPE` to see valid choices inline — they appear
in the param line as a `[a | b | c | ...]` list. Example:

```
blending_method: Enumeration  [add | maximum | maximum_smooth | minimum | multiply | replace | substract | ...]
```

Set the param in your spec exactly as the choice string shown:

```json
{"id": "blend", "type": "Blend", "params": {"blending_method": "maximum"}}
```

**Escape hatch:** a small number of `Enumeration` params are not yet catalogued (e.g. the
`method` param on `CloudRandom`). For these, `hsd nodes --show TYPE` prints
`(pass a full value-object dict)` and validation will report the issue. Use a full value-object
dict in that case, copying the format from a `.hsd` file produced by the GUI.

**Advanced types** — `Color gradient`, `Cloud`, `Path`, `Filename`, and any other type not in
the table above — require passing a full value object (a dict matching Hesiod's internal
representation). Do not attempt to set these with a scalar or simple array. Look at `.hsd` files
produced by the GUI for reference.

---

## Catalog query cheatsheet

Discover node types and their ports/params before writing a spec. Never guess type names or port
names.

```bash
# List all available node types
PYTHONPATH=scripts python3 -m hsd nodes

# Search by keyword
PYTHONPATH=scripts python3 -m hsd nodes --search Noise

# Browse by category
PYTHONPATH=scripts python3 -m hsd nodes --category Erosion

# Show ports and params for a specific type (always run this before wiring)
PYTHONPATH=scripts python3 -m hsd nodes --show NoiseFbm
```

Example `--show` output for `NoiseFbm`:

```
NoiseFbm  [Primitive/Coherent]
  Fractal noise is a mathematical algorithm used to generate complex and detailed patterns characterized by self-similarity across different scales.
  ports:
    input  control: VirtualArray
    input  dx: VirtualArray
    input  dy: VirtualArray
    input  envelope: VirtualArray
    output output: VirtualArray
  params:
    kw: Wavenumber
    lacunarity: Float
    noise_type: Enumeration  [OpenSimplex2 | OpenSimplex2S | Perlin | Perlin (billow) | Perlin (half) | Value | Value (cubic) | Value (linear) | Worley | Worley (doube) | Worley (value)]
    octaves: Integer
    persistence: Float
    post_gain: Float
    post_gamma: Float
    post_inverse: Bool
    post_remap: Value range
    post_saturate: Value range
    post_smoothing_radius: Float
    seed: Random seed number
    weight: Float
```

The output shows each port's direction (`input`/`output`), name, and `data_type`; and each param's
name and type string. Use the type string to look up the JSON form in the scalar-types table above.
Note that `noise_type` is `Enumeration` — its valid choices are listed inline; pass a plain string
(e.g. `"Perlin"`) in your spec and the toolkit resolves it automatically.

---

## Port-datatype rule

Every link must connect ports of the **same `data_type`** (`VirtualArray` or `VirtualTexture`).
These types are incompatible and cannot be mixed in a single link.

The bridge nodes (`ColorizeGradient`, `ColorizeSolid`) accept a `VirtualArray` input and emit a
`VirtualTexture` output; they are the only way to cross the type boundary.

For the full explanation with port tables and a fork diagram, see `SKILL.md §4`.
