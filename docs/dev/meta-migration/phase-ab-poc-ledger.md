# SDD Progress — Meta Migration Phase A + B

Plan: docs/superpowers/plans/2026-07-09-meta-migration-phase-ab.md
Branch: feature/meta-migration
Branch base SHA: 34454aa3

Verification model (env-adapted, per repo precedent): subagents write + inspect-verify
per task; controller runs consolidated build via `nix develop /home/barrulus/quixote#cpp-qt-desktop`
at phase boundaries; headless `hesiod --inventory` for runtime checks; GUI visual smoke is user-driven.

Working tree caveat: external/{GNode,GNodeGUI,HighMap} show as M (user's checkout deltas) +
untracked files — NEVER stage them. Each task stages only its own named files.

## Tasks
- [x] A1: Add Meta as submodule (frozen pin)
- [x] A2: Wire Meta into CMake build + prove it links
- [x] B1: Opt-in meta::ContainerGroup on BaseNode
- [x] B2: Author Noise node params on Meta
- [x] B3: Render Noise panel via meta_qt
- [x] B4: Serialize meta-backed Noise (build green; round-trip pending user) (both shapes, decide)
- [x] B5: Facade-vs-codemod decision doc

## Ledger
(base 34454aa3)
Task A1: complete (commit 34454aa3..93e42ed3, Meta pinned e71e798; staged only .gitmodules+external/Meta, review clean)
Task A2: complete (commit 93e42ed3..d1a010f2, review clean).
  PHASE A GATE PASSED: meta (12/12 -> libmeta.a) + meta_qt (22/22 -> libmeta_qt.a) compile in
  Hesiod's Qt6 toolchain; hesiod binary LINKS with meta+meta_qt (384/384, build/bin/hesiod).
  CAVEAT: headless `--inventory` run hit a Qt runtime skew (libQt6Qml Qt_6_PRIVATE_API undefined
  symbol = qtdeclarative 6.11.0 vs built-against Qt6 mismatch). ENVIRONMENTAL, not from Meta
  (our libs don't link QML). Runtime/GUI verification is user-driven; B3/B4 runtime checks need
  user's working Qt runtime env.
Task B1: complete (commit d1a010f2..4cc249c8, review clean; uses_meta/meta_group accessors match contract, legacy attr map untouched)
Task B2: complete (commit 4cc249c8..d83af78d, review clean; 4 attrs -> meta container w/ ui metadata, post-process kept legacy, compute reads via c.value<T>). BUILD GREEN (hesiod links, meta API compiles).
Task B3: complete (commit d83af78d..c99576e2, review clean; setup_layout branches on uses_meta -> meta::qt::ContainerGroupWidget, value_changed->recompute wired; setup_connections null-guard pre-existing). BUILD GREEN.
Task B3: GUI-VERIFIED (user, 6.11.1 rebuild). Meta panel renders: Settings>Main Parameters/Tiling
  collapsible groups (ui.category), Type=Perlin dropdown, Spatial Frequency X/Y linked+locked
  (x==y confirmed), Seed, Periodic checkbox; terrain recomputes. FUNCTIONAL PASS.
  FINDING (logged, deferred per user): Meta slider/LinkedSliders minimumWidth exceeds Hesiod's
  hard-capped 360px settings panel (node_settings_widget.cpp:28-29 "TODO fix this",
  ScrollBarAlwaysOff) -> widest widget (kw paired sliders) forces container wide, all inputs clip.
  Proper fix = Meta widget sizing / panel cap, belongs to Meta-editing phase. Empty toolbar
  buttons = qtsvg launch-env (SVG icons), not Meta.
ENV NOTE: nixpkgs bumped Qt 6.11.0->6.11.1 overnight; stale build/ (CMakeCache 6.11.0 RUNPATH)
  caused Qt_6_PRIVATE_API symbol skew. FIX: rm -rf build + fresh configure under devshell ->
  coherent 6.11.1 binary. Full rebuilds must be capped (-j4) + detached (unbounded -j OOMs, killed foot).
Task B4: code-complete, BUILD GREEN (commit b2659356; base_node.cpp json_to/json_from add native
  _meta shape for uses_meta nodes + fail-loud else-guard when _meta absent). DECISION: native shape
  chosen over adapter -- adapter can't reproduce legacy semantic .hsd type tags (Enum/Seed/Wavenumber)
  from Meta's plain value-types (int/vec2/bool) without a bespoke per-attr semantic map. Full-migration
  implication (Python hsd toolkit + site/examples/*.hsd assume legacy shape) = documented finding.
  ROUND-TRIP VERIFICATION: pending user GUI save/reload + controller .hsd inspection.
Task B5: complete (controller doc task). Decisions recorded in design spec §11: serialization=native
  _meta; facade over codemod (5-8 lines/attr direct vs 1-line legacy -> facade shim maps legacy
  add_attr type -> meta add<T>+metadata); widget-width + env notes captured.

B4 ROUND-TRIP WRITE VERIFIED (user save /output/meta-noise-test.hsd): _meta block present at
  graph_manager/graph_nodes/graph/nodes[4]; saved values match user input exactly
  (kw={4,4}, noise_type=6, periodic=true, seed=42). Read side = user reload confirm (pending).
  FINDINGS: native _meta is heavily BLOATED (serializes full metadata tree incl. nested
  snapshot_manager, not just values) + a stray ui.state entry -> future refinement (serialize
  values only). Logged, not blocking.

BUGFIX (found in GUI verification, commit follows fix-build): fresh Noise node rendered FLAT until
  a value change. Root cause = B2 defaulted noise_type to raw int 0, not a valid hmap::NoiseType;
  GraphNode::add_node DOES compute on add (line 53 node->compute()) but with 0 -> flat. Re-selecting
  the shown combo item emits no change (Qt) so flat persisted. FIX: default = (int)NoiseType::SIMPLEX2
  (matches displayed 'OpenSimplex2', valid, consistent under either combo-sync behavior).

## FINAL WHOLE-BRANCH REVIEW (opus): 1 crash found+fixed, rest accepted
- F1 CONFIRMED crash (HIGH): meta branch nulls attributes_widget but create_toolbar's 5 state/preset
  buttons deref it unconditionally (add_toolbar defaults true) -> click crashes Noise node.
  FIXED commit (finding1): guard each lambda `if (this->attributes_widget)`.
- F2 low: kw max capped 64 vs legacy FLT_MAX. ACCEPTED (sane slider bound; FLT_MAX impractical).
- F3 by-design: pre-branch .hsd Noise nodes load to defaults WITH loud log (= intended fail-loud;
  documented full-migration concern, not a bug). Round-trip within branch sound.
- F4 latent: const meta_group() unguarded but safe behind sole gated caller (json_to). Left as-is.
- Verified clean: Meta API usage vs frozen headers, legacy path intact, commit hygiene (only 8+2 fix
  files; dirty external/{GNode,GNodeGUI,HighMap} + untracked NOT committed).
