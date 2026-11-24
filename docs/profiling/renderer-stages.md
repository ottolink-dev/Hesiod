# Renderer CPU stages and GPU frame spans

Step 3 adds optional instrumentation inside QTerrainRenderer. Hesiod forwards
`HESIOD_ENABLE_TRACY` to `QTR_ENABLE_TRACY`; standalone QTerrainRenderer defaults
to OFF. All translation units consuming its public renderer class receive the
same compile definition. The enabled targets share the installed Tracy client.
Normal builds omit the hooks, GPU-query fields and GPU profiler implementation.
The dependency is reviewed in
[QTerrainRenderer PR #43](https://github.com/ottolink-dev/QTerrainRenderer/pull/43),
pinned here at `867a874f708a2f7ea8cff10bcf2bf513686c17a3`.

## CPU stages

- `QTR::heightmap.worker` and `QTR::heightmap.build` cover the background worker
  and geometry construction. Submission, worker and delivery metadata carry the
  renderer's numeric address and request number. The worker uses only a captured
  address value, not mutable widget state.
- `QTR::heightmap.deliver` marks the GUI callback and records whether the request
  was accepted or superseded/deleted. It is distinct from queue waiting time.
  Addresses and request numbers can be reused after destruction; correlate them
  within a widget lifetime and time range, not as globally unique identifiers.
- `QTR::apply_heightmap_geometry`, `QTR::mesh.upload` and `QTR::texture.*` expose
  geometry application and CPU time in buffer/texture creation calls. The driver
  can defer GPU work after these calls return.
- `QTR::paintGL`, scene/UI functions, shadow/depth passes, skybox and void-grid
  functions expose CPU frame preparation and rendering submission. Nested times
  include children and must not be added together.

## GPU plots

Each renderer context has a distinct `QTR GPU paintGL ms #N renderer=...` plot.
These are **Tracy numeric plots, not native Tracy GPU timeline zones**. They use
OpenGL timestamp queries around `paintGL`'s scene/UI work and report elapsed GPU
command-stream time, which can include gaps waiting for CPU submission. They are
not a GPU busy-time counter, display latency, swap interval, compositor duration,
or GPU upload/kernel timings outside that span.

Each context owns eight query pairs. Collection checks availability before reading
results; there is no `glFinish`, polling wait or synchronous query-result wait.
A full pool skips a measurement rather than blocking rendering. Old-connection
results are discarded after an on-demand capture reconnect. No new queries are
submitted while disconnected. Context teardown deletes queries while the context
is current; context loss leaves cleanup to OpenGL if it can no longer be made
current. Unsupported timestamp counters leave CPU profiling available.

Samples appear when collected, potentially several frames after submission; do
not align their plot timestamps directly with CPU frame starts. An idle viewer
may retain the final pending samples until another repaint, and teardown can
discard them. The application log records the actual OpenGL vendor/renderer,
plot identity, timestamp counter support, completed sample count, skipped frames
and pending samples at teardown. Plot labels remain alive for the process because
Tracy retains their string pointers (one small label per context lifetime).

## GUI validation workload

Build and run `profile_viewer` as described in [the viewer guide](viewer-workload.md).
After selection, three parameter changes, a camera drag and a 2D rendering phase,
it now creates and
destroys two secondary viewers while keeping the original viewer alive. It forces
repaints in both contexts and checks for OpenGL errors, then repaints the surviving
viewer. Use a real desktop/OpenGL display; offscreen CTest cannot validate this.

Inspect CPU zones during camera movement and confirm GPU samples for all three
context lifetimes. Background heightmap construction should occur during data
updates, not camera-only motion. The workload still writes its initial/final
framebuffer and thumbnail images. It checks context switching and widget lifetimes;
it does not simulate hardware context loss. Keep captures separate from Day 1.


## Validation (2026-10-19)

The full Tracy-enabled application and GUI workload built successfully; the
`graph_editor` and `profiling_highmap` CTests passed. A standalone renderer build
with `QTR_ENABLE_TRACY=OFF` also passed and its static library contains neither
Tracy nor GPU-profiler symbols. Disabled-hook compilation passed for the ten
modified/new renderer implementation files and the GUI workload.

A real XCB capture on Mesa Intel(R) Graphics (ARL), OpenGL 4.6, recorded 113 CPU
paint spans and 110 GPU samples across three context lifetimes (78, 16 and 16).
No measurements were skipped; each context retained one final pending sample at
teardown. The two secondary widgets reused an address, while their GPU plot
identities remained distinct. The workload completed with no OpenGL errors.

The capture contains 2D/3D scene and UI zones, shadow/depth passes, mesh upload,
background build and GUI delivery zones. Camera motion produced 21 paint spans
without viewer data refreshes or heightmap rebuilds. A second real desktop run
without a connected profiler also passed, with zero GPU samples and zero pending
queries at teardown. The initial framebuffer was visually checked.

These are instrumentation and lifecycle checks, not a new performance baseline.
GPU timings on NVIDIA, forced hardware context loss, capture reconnect and query
pool saturation were not exercised by this workload. The latter cases have
explicit handling in the implementation but are not claimed as tested here.
