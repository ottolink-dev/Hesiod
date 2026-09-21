# PR #758 Linux investigation

PR: https://github.com/ottolink-dev/Hesiod/pull/758
Initial tested head: `3ad443e3903fac2f841a0252d12a57ba111575f6`.
The fractional Wayland follow-up below tests `8dc0e2d8` and the graph redraw fix.
Environment: Linux x86_64, Qt 6.11.2, GCC 15.3.0; Xvfb at 1920x1080,
software OpenGL. Separate spot checks used XWayland and native Wayland on
the local desktop. This does not cover Otto's particular Qt version, GPU,
desktop, or mixed-monitor setup.

## Current workaround: fractional window redraws

The graph-only and popup-only fixes below did not cover the docked node library.
A further user recording at output 100% / application 90% shows retained hover
strips in that tree. The actual application reproduces this: moving over 80
positions in the tree leaves 11,151 pixels that change after a forced redraw.
At 150% / 90% it leaves 634 stale pixels; at 150% / 100% the baseline is clean.

The application now installs `FractionalRepaintFilter` in place of the
menu-only filter. On a pending QWidget window UpdateRequest, it checks that
window's current DPR and expands the dirty region to the full window when the
DPR is fractional. This covers child widgets, graphs, dialogs and popups through
their backing store. The explicit FullViewportUpdate setting in GraphNodeWidget
is removed: the common workaround handles that case too. Integer-DPR windows
keep Qt's normal partial updates, and idle windows do not acquire a redraw loop.
No scale is clamped and no undocumented Qt environment variable is enabled.

The tradeoff is more painting per update for fractional-DPR windows. This is a
correctness workaround; large graphs and continuously animated 3D scenes have
not been performance-benchmarked. The DPR is read per update, rather than cached
at startup, but actual mixed-monitor transitions remain untested.

### Current validation

The full application rebuilt with Qt 6.11.2. Replacement diagnostic mains link
the updated production objects without adding a second redraw workaround.
The graph diagnostic confirms the original MinimalViewportUpdate mode is active.
All 15 compositor screenshot comparisons have zero changed pixels above the
status bar:

| Output / application | Node-list hover | Node movement | Pan | Zoom | Popup hover |
| --- | --- | --- | --- | --- | --- |
| 100% / 90% | Pass | Pass | Pass | Pass | Pass |
| 150% / 90% | Pass | Pass | Pass | Pass | Pass |
| 150% / 100% | Pass | Pass | Pass | Pass | Pass |

This includes the previously failing clipped-node case at 100% / 90%. Those
older failures below describe the earlier implementations, not the current one.
The comparisons use the same settled-versus-forced-redraw method, with the top
760 rows of 1280x800 compositor captures excluding the changing status bar.

The palette suite now also compares child-widget paint regions with an
unfiltered baseline, checks integer-DPR behavior, and checks idle behavior for
both ordinary windows and menus. Coverage assertions use region subtraction:
QRegion::contains(QRect) only tests overlap and was too weak in the earlier menu
checks. The corrected tests exercise complete coverage of the menu/window background;
Qt may retain an opaque child's unchanged pixels, so the window test explicitly
allows that optimization. The complete suite passes at scale factors 0.5, 0.9,
1, 1.25, 1.35, 1.5, 2, and 3 on the isolated X11 display.

Local tools and captures are in `/tmp/hesiod-758-combined-scale`:
`run-window-fixed-nested.py` runs the 15 visual cases (`window-fixed-*`), while
`run-window-tests.py` runs the palette suite (`window-palette-*`).
`build-probes.py` builds the tree, graph and menu diagnostics against the
production application objects. The earlier baseline captures remain in
`app-tree-*` and `fixed-*`.

## Initial graph-only fractional redraw fix (2026-09-17)

A recording on NixOS/niri provides a repeatable case: start Hesiod at 100%,
set output scaling to 150%, then save application scaling at 90% and restart.
Moving/adding nodes leaves black outlines behind. Returning output scaling to
100% makes the user's recorded case work again. This reproduces with Qt 6.11.2;
an old Qt version is not a sufficient explanation.

A minimal Qt window containing a QGraphicsView and a sibling QOpenGLWidget
reproduces stale pixels without Hesiod's graph or terrain code. At output 150%
and application 90%, the actual window DPR is 1.35, while QScreen reports 1.8.
Screen DPR alone is therefore not a reliable basis for clamping application
scale. An effective scale below 100% is not required to trigger this defect.

GraphNodeWidget now uses QGraphicsView::FullViewportUpdate. Moving items under
the default MinimalViewportUpdate leaves pixels which disappear after a forced
full viewport redraw; using FullViewportUpdate prevents those trails. This is
a local workaround for partial redraw corruption, not a proven diagnosis of the
precise rounding error inside Qt. It also applies after changing output scale
or moving the window to another display, without a startup-only DPR decision.
The tradeoff is repainting the entire graph viewport on scene changes, which
may cost more on large graphs. Large-graph performance has not been measured.

### Rendering regression checks

An isolated nested niri compositor hosted by Xvfb avoids changing the user's
real desktop. The full application diagnostic links Hesiod's production objects
with a replacement main, creates a real Noise node, and performs 80 movement,
panning, or zoom steps. It captures the compositor output after settling, forces
one complete viewport redraw, and captures it again. Capturing through niri
avoids QWidget::grab(), which would itself repaint and hide the defect.

On the unpatched application, output 150% / application 90% leaves a 230-pixel
vertical trail which disappears after the forced redraw. At 150% / 100%, there
are no stale graph pixels. The automated movement sequence also exposes stale
pixels at 100% / 90%, unlike the user's recording: exact geometry and movement
matter, so the failure must not be treated as exclusive to one scale pair.

With the production fix, eight of the nine before/after screenshot comparisons
match exactly above the status bar. The remaining case is recorded explicitly:

| Output scale | Application scale | Moving nodes | Panning | Zooming |
| --- | --- | --- | --- | --- |
| 150% | 90% | 0 changed pixels | 0 changed pixels | 0 changed pixels |
| 150% | 100% | 0 changed pixels | 0 changed pixels | 0 changed pixels |
| 100% | 90% | 5,378 changed pixels; clipped node top | 0 changed pixels | 0 changed pixels |

The comparison covers the top 760 rows of each 1280x800 compositor capture;
the graph viewport is entirely inside that region. The bottom status bar is
excluded because it updates between screenshots independently of graph redraws.
At 100% / 90%, the moving-node case still leaves the top of a node undrawn until
another repaint. This defect was also present before the fix (13,363 changed
pixels in that comparison); the workaround improves it but does not eliminate
it. The 150% / 90% case from the recording passes all three motions.

As a diagnostic only, running the same patched application with
`QT_WIDGETS_HIGHDPI_DOWNSCALE=1` makes both 100% / 90% and 150% / 90% moving-node
comparisons match exactly. This points to Qt backing-store scaling in the
remaining case. This undocumented environment option is not enabled by the
production change. Results are in `downscale-*` beside the other captures.
The application rebuilt successfully. The previous palette tests do not cover
this renderer; their earlier passes are not used as evidence for this fix.

To repeat the manual regression on the actual desktop:

1. Start at application 100%, change output scaling to 150%, and add/move nodes.
2. Save application 90%, exit normally, and restart. Move nodes repeatedly,
   pan, zoom, select nodes, and browse the palette. Check for retained outlines.
3. Exit, set output scaling to 100%, and restart at application 90%. Repeat.
4. Check switching output scaling while the window is open. If multiple
   monitors are available, also move the window between different scales.

The automated cases use Qt 6.11.2, native Wayland and software OpenGL. Qt 6.4.2,
X11 reproduction of the original report, actual mixed-monitor transitions,
and large-graph performance remain unverified. This fixes the reproduced graph
trails; it does not establish that every reported widget/layout issue is fixed.

Local diagnostic artifacts are in `/tmp/hesiod-758-combined-scale`: `app-*`
contains baseline comparisons, `fixed-*` contains production-fix comparisons,
and `run-fixed-nested.py` runs the nine cases. The diagnostic main is
`/tmp/hesiod-758-ui-harness/app_probe.cpp`; compilation/link scripts reuse the
full application's build commands. These are local investigation tools, not a
portable CI test. Screenshots are `partial.png` and `full.png` in each case.

Qt references:

- https://doc.qt.io/qt-6/qgraphicsview.html#ViewportUpdateMode-enum
- https://doc.qt.io/qt-6/qscreen.html#devicePixelRatio-prop
- https://doc.qt.io/qt-6/highdpi.html

## Initial popup-only hover fix (2026-09-17 follow-up)

After the graph fix, a screenshot and follow-up confirm that hovering between
node-creation menu categories leaves horizontal lines and broken border segments
at application 90% / niri output 150%. The graph viewport workaround does not
cover QMenu popup windows.

A standalone Qt menu with the same style characteristics reproduces the lines:
1,604 pixels differ between the settled hovered menu and a forced full redraw at
150% / 90%. At 150% / 100% the comparison matches. This does not require the
Hesiod graph renderer or its OpenGL sibling.

`MenuRepaintFilter` is installed once on the GUI application and covers menus
and submenus created later, including the graph library's plain QMenu objects.
On a pending UpdateRequest, it expands a menu's dirty region to the full popup
if that menu's current DPR is fractional. Qt then handles the request normally.
It neither intercepts Paint nor schedules recurring redraws; integer-DPR menus
and non-menu widgets retain their existing update behavior. The tradeoff is a
full popup repaint when a menu item or child widget changes.

Validation:

- The standalone menu comparison has zero changed pixels at output/app scales
  150% / 90%, 150% / 100%, and 100% / 90% with the workaround.
- A diagnostic linked against the updated production application opens the
  real node-creation menu and changes its active category 60 times. Full
  compositor screenshots before/after forcing another redraw match exactly
  at all three scale pairs, including the visible submenu.
- The palette suite now checks that hover and small-region updates produce a
  full popup paint at fractional DPR and that a settled popup remains idle.
  The complete suite passes in eight isolated X11 processes at scale factors
  0.5, 0.9, 1, 1.25, 1.35, 1.5, 2, and 3. The application rebuild also passes.

Local captures are `menu-*` and `app-menu-*` under
`/tmp/hesiod-758-combined-scale`; palette logs are in `palette-*` there.
At this stage the node clipping at output 100% / application 90% was
outside the menu-only fix and remained unresolved; see the current workaround
above for the subsequent passing result. No private Qt environment setting
is enabled by the production workaround.

## Confirmed startup config bug

`ui_scale::executable_dir()` used the running executable's location on Windows,
but resolved `argv[0]` relative to the working directory on Linux. Launching
through PATH or a symlink could therefore read the scale from a different
config than `AppContext` subsequently loads. This is separate from the reported
rendering corruption and can also prevent a saved scale from being applied.

The Linux implementation now resolves `/proc/self/exe`, retaining the existing
fallback when procfs is unavailable. The executable-path tests already present
in the PR failed four checks at every tested scale before this change. An
additional assertion covers resolving the path before QApplication exists.

## Existing suite

Built the PR's `tests/ui_palette` sources with a temporary standalone CMake
wrapper, using Qt Widgets/Test, spdlog, and nlohmann_json. After the path fix,
all 152 checks passed in each of seven separate Xvfb processes with
`QT_SCALE_FACTOR` values 0.5, 0.9, 1, 1.25, 1.5, 2, and 3.

The suite does not construct the real graph editor, node settings, or application
settings content. Passing it alone cannot establish that the reported problem
is fixed. Offscreen mode also does not exercise OpenGL composition.

On the tiling desktop, two XWayland dialog geometry checks failed at 0.9;
they passed on the isolated Xvfb display. One native Wayland flyout-open check
failed. Those results should not be silently counted as passes or confused with
a reproduction of the main-window corruption.

## Initial rendering diagnosis (before the fractional Wayland reproduction)

The attached report video shows corruption across the main window, not just a
collapsed node settings splitter. A minimal Qt/OpenGL window renders correctly
at 0.9 on the isolated X11 display with this Qt version.

The previous rounding-policy explanation is not established: Qt 6 already uses
PassThrough by default, and QT_SCALE_FACTOR is applied without rounding.
Qt's source also documents limitations for factors below 1 when clamping
screen scale factors. This warrants testing the actual affected platform; it
is not, by itself, evidence that every sub-100% setting fails.

References:

- https://doc.qt.io/qt-6/highdpi.html
- https://github.com/qt/qtbase/blob/6.11/src/gui/kernel/qhighdpiscaling.cpp

## Initial full application results (before the fractional Wayland reproduction)

The full application built successfully with all pinned submodules unchanged:

```sh
cmake -S /tmp/hesiod-758 -B /tmp/hesiod-758-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DHESIOD_ENABLE_GENERATE_APP_IMAGE=OFF \
  -DHESIOD_ENABLE_PCH=ON -DHIGHMAP_ENABLE_BENCHMARKS=OFF
cmake --build /tmp/hesiod-758-build -j 16
```

A temporary diagnostic executable links the actual application objects with a
small replacement main. It applies the persisted scale before constructing
HesiodApplication, creates a Noise node, clicks its header through X11 using
xdotool, checks selection and populated NodeAttributesWidget content, opens
the actual AppSettingsWindow inside ScrollableDialog, and captures the windows.
It uses an isolated portable config beside the test binary. OpenGL reported
Mesa llvmpipe, OpenGL 4.6 compatibility profile, LLVM 21.1.8.

- At 0.9, 1, 1.25, and 1.5, native node selection and settings population pass,
  and the full application settings dialog fits the screen.
- At 0.9, the node settings pane is also entirely inside the screen. The
  main-window corruption from Otto's video is **not reproduced** here.
- At 2 and 3, native node selection and settings population still pass once
  the node header is scrolled into view, but the node settings pane extends
  beyond the screen. The main window's layout minimum exceeds the available
  logical width. `Viewer::Viewer` sets a minimum size from the viewer settings;
  clamping the main window before layout does not overcome those constraints.
  This is an outstanding high-scale usability issue, not evidence of the 0.9
  corruption. The production layout was not changed in this investigation.

At this stage only the executable-path bug had been fixed, and the rendering
problem remained unreproduced. The later native Wayland investigation and graph
redraw workaround above supersede that conclusion. The initial 200%/300%
layout overflow remains a separate unresolved issue.

## Local artifacts

- Worktree: `/tmp/hesiod-758`, branch `fix/758-linux-ui-scaling`.
- Full build: `/tmp/hesiod-758-build`.
- Diagnostic sources and scripts: `/tmp/hesiod-758-ui-harness`.
- Test logs and images: `/tmp/hesiod-758-results`.
