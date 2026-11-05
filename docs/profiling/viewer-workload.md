# Viewer and thumbnail profiling

The viewer zones complement node computation timing. They measure Hesiod's
CPU-side preparation and calls into QTerrainRenderer. Enable the same optional
Tracy build described in [the profiling guide](README.md).

## Zone boundaries

| Zone family | Work included |
| --- | --- |
| `Viewer::set_current_node_id`, `Viewer::update_widgets` | Selection and widget refresh, including nested renderer updates |
| `Viewer::queue_renderer_update` | Posting a refresh from node completion to the GUI thread; this is not the queue wait time |
| `Viewer3D::update_renderer` | One viewer refresh; valid-node metadata identifies viewer address, node ID/type, source dimensions and mesh cap |
| `Viewer3D::elevation.to_array` | Materialising/resampling the display heightmap, with source and display dimensions on its parent zone |
| `Viewer3D::elevation.submit_async` | Submitting heightmap geometry to the renderer's background job; **not** the subsequent mesh build/upload |
| `Viewer3D::water.*` | Elevation/depth materialisation, water-surface preparation and synchronous renderer call |
| `Viewer3D::color.*`, `Viewer3D::normal.*`, `Viewer::selector_image` | Heightmap/texture conversion into display pixels |
| `Viewer3D::albedo.renderer_call`, `Viewer3D::normal.renderer_call` | Time spent inside the texture setter, including whatever synchronous work it performs |
| `Viewer3D::*prepare_and_submit` | Point/path/tree/rock data extraction and renderer calls |
| `Viewer3D::set_skybox` | Skybox loading/preparation and renderer handoff |
| `DataPreview::*` | Thumbnail materialisation/resampling, colourisation, image copy, pixmap/border creation, clear/dim operations |

Parent times include their children: do not add nested durations together.
Renderer setter timings do not establish GPU execution or completion times.
The [renderer stage instrumentation](renderer-stages.md) adds background mesh
work, CPU render stages and per-context GPU frame-span plots underneath these
viewer zones.
Early-return/clearing viewer refreshes have no valid-node metadata. Exclude them
when summarising the cost of refreshing a populated view. Valid-node refreshes
can also have no active layer ports; use the child layer zones to distinguish
actual data preparation from selection/routing changes.

Queue-post zones carry only the viewer address: the callback may run on a worker
thread, so instrumentation must not read mutable GUI selection state there.
Addresses identify viewers within one process, not across captures.

## Repeatable GUI workload

Build the explicit diagnostic target with tests and Tracy enabled:

```sh
cmake -S . -B build/tracy -DHESIOD_ENABLE_TESTS=ON -DHESIOD_ENABLE_TRACY=ON \
  -DTracy_DIR=/path/to/tracy-install/lib/cmake/Tracy
cmake --build build/tracy --target profile_viewer --parallel
```

This target requires a real desktop/OpenGL display and is deliberately not an
offscreen CTest. It creates an isolated viewer with temporary settings, computes
a 512² GaussianPulse with one OpenMP thread, caps the display mesh at 256,
creates a 128² thumbnail, changes the radius three times, then sends a camera
drag. The same heightmap supplies the synthetic elevation, water-depth and colour
layers to exercise their preparation paths. It also creates and destroys two
secondary viewers to exercise multiple OpenGL contexts. This is a diagnostic fixture, not a
physically meaningful water scene. The window closes after the workload.

Start capture in another terminal (use a fresh output path):

```sh
tracy-capture -a 127.0.0.1 -p 8108 -s 30 -o viewer.tracy
```

Then run from the executable directory so application data can be found:

```sh
cd build/tracy/bin
TRACY_PORT=8108 TRACY_NO_SYS_TRACE=1 HESIOD_TRACY_WAIT=1 ./profile_viewer
```

A successful run returns zero and writes `viewer-profile-initial.png`,
`viewer-profile.png` and `viewer-preview.png` in its working directory. It fails
if capture connection times out, the window cannot be exposed, OpenGL is invalid,
or image output fails. For isolated output, run in a new directory with a `data`
symlink to the executable's adjacent data directory.

Export and inspect the capture:

```sh
tracy-csvexport -u -p viewer.tracy > viewer-zones.csv
```

Check for the elevation, water, colour and thumbnail zones, three queued refreshes
following parameter changes, and all three `Viewer workload::*` phase names.
Those **phase spans include deliberate event-loop settling waits**; they are
markers, not computation or frame-time benchmarks. Camera motion does not
normally rebuild node data, so its phase may contain no preparation zones.

Keep captures separate from the [Day 1 node baseline](baselines/2026-10-08/README.md).
Record source/dependency revisions, build flags, viewport size, mesh cap, actual
OpenGL vendor/renderer from the application log, display backend and workload.
The OpenGL display GPU can differ from the OpenCL compute device. For real-project
captures, also record visible viewers, selected/pinned nodes and preview modes.


## Initial validation (2026-10-19)

The Tracy-enabled application and workload built successfully, and the graph-editor
CTest passed. All five changed/new translation units also compiled with profiling
disabled and produced no `tracy::` symbols.

A real desktop capture using XCB and Mesa Intel(R) Graphics (ARL), OpenGL 4.6,
confirmed elevation conversion/submission, water preparation, colour conversion,
selector-image and thumbnail zones. It recorded three queued updates for the three
parameter changes and no viewer data refreshes during the camera-drag phase.
This checks instrumentation and workload behaviour; it is not a stable performance
baseline or GPU-frame measurement. Texture, point, path, tree and rock branches
have hooks but were not exercised by this heightmap workload. The October 8 node
baseline remains unchanged.
