# Local optimization issue drafts

These proposed follow-ups to [Hesiod #783](https://github.com/ottolink-dev/Hesiod/issues/783)
are kept local as requested; none have been posted to GitHub. The
[15-capture allocation study](../assessments/2026-11-27/allocations/README.md)
contains exact disjoint allocation groups and raw evidence. Native RSS and heap
allocation measurements must remain separate comparisons.

## 1. HighMap / Assimp: reduce export scene duplication and vertex-joining memory

**Trigger:** ExportAsset serializes a regular 1024² triangle mesh. Both the
masked full fixture and the elevation-only unmasked fixture incur substantial
Assimp preparation memory, even though their HighMap meshes already use indexed
vertices.

**Measured evidence:** in the full fixture, 343,058,000 live bytes at the global
heap peak come through `Assimp::JoinVerticesProcess`, 83,777,319 through scene
copying, and 83,771,404 remain in the original HighMap mesh. Those disjoint groups
coexist within the 649,672,003-byte process heap peak. The unmasked minimal case
has almost identical JoinVertices/scene-copy groups. The masked coordinate hash
map is **not live at this peak**; its removal alone would not remove the dominant
observed allocations.

**Proposed investigation:** identify which Assimp format/preprocessing requirements
cause JoinVertices and the scene copy. Assess whether already-indexed grid meshes
can avoid redundant preparation or transfer ownership through a supported API.
Do not simply disable required export processing or replace per-face buffers
without checking Assimp ownership/destruction rules. Map indexing is a separate
local optimization candidate, not the main peak-memory explanation. Streaming
exports require a separate API and format design.

**Acceptance:** equal selected vertices/faces, UVs, materials and normals for
masked/unmasked, empty/sparse/dense masks, both boundary policies and supported
formats; measured reduction of allocations actually live at global heap peak;
512²/1024²/2048² RSS and heap comparisons with identical fixtures and toolchain.

## 2. HighMap: transfer final quilting secondary outputs instead of copying

**Trigger:** TextureQuiltingExpand with guide RGBA plus A–D supplies 20 secondary
arrays. `quilting` copy-assigns its secondary output arrays back into the supplied
working arrays immediately before destroying the output vector.

**Measured evidence:** at the full 1024² process heap peak, two distinct sets of
116,741,120 bytes coexist: output construction at `quilting.cpp:74` and copy
assignment at `quilting.cpp:222`. Each is `20 × 1208 × 1208 × 4` bytes. At 512²,
each duplicated set is 28,800,000 bytes. These are global-peak-live bytes, not
independent stack maxima. The wrapper still retains the original gathered full
inputs while this overlap occurs.

**Proposed change:** investigate moving the no-longer-needed secondary outputs
into their destinations, reviewing duplicate/aliased pointer behavior first.
Assess follow-on crop allocation and `get_random_patch`'s lvalue `push_back` of
its temporary patch separately. Preserve deterministic patch choices, seam
smoothing and exact secondary-channel correspondence.

**Acceptance:** deterministic equality for guide and all secondary channels,
rectangular sizes, fractional expansion, patch transforms and pointer aliasing;
fewer payload allocation/copy bytes; repeatable heap/RSS comparison with 0, 4 and
20 secondary channels. Do not promise the full 111.333 MiB as peak reduction:
after eliminating this overlap, the peak can move to an earlier phase.

## 3. HighMap: reduce TextureQuiltingShuffle channel/strip workspace

**Trigger:** a full 1024² Shuffle fixture gathers a height guide and 20 secondary
channels, then creates all output, horizontal strip and patch buffers together.

**Measured evidence:** 88,080,384 bytes in gathered arrays and 209,465,088 bytes
in core quilt output/strip/patch paths are live at the 381,992,017-byte heap peak.
Secondary output and horizontal-strip payloads alone are 114,624,720 and
55,828,080 bytes. Final secondary-copy-assignment storage is not live at this
peak; the ownership-transfer change above is not established as a fix for this
Shuffle peak. The minimal guide-only fixture peaks at 139,906,202 bytes.

**Proposed investigation:** bound simultaneous secondary-channel or strip storage,
reuse scratch where lifetimes permit, and consider staged secondary-channel
processing using recorded patch/seam decisions. Independently quilting graph
tiles would change cross-tile seams and is not an equivalent implementation.

**Acceptance:** exact output or an explicitly reviewed numerical tolerance across
all secondary channels and seams, deterministic repeats, bounded workspace with
channel count/resolution, and captured lower live-heap peaks. Test 1×1/4×4 graph
tiles and RAM/disk storage without claiming that tile storage alone removes the
full-array algorithm working set.

## 4. Hesiod: release ExportAsset normal-map working arrays before mesh export

**Trigger:** ExportAsset writes its normal-map PNG, then retains the generated
normal texture, three copied channels, alpha and virtual normal texture during
mesh construction/serialization.

**Measured evidence:** the full 1024² global peak includes 12 MiB in nx/ny/nz
copies at `export_asset.cpp:190–192`, 4 MiB in alpha at `:193`, 12 MiB in generated
normal channels and 16 MiB in virtual normal channels. Together this is **44 MiB
of pixel payload**, plus 736 bytes of attributed metadata. At 512² the same
families total 11 MiB plus 736 bytes. This confirms the source lifetime audit.

**Proposed change:** end the normal-map construction/blending/writing scope before
calling `export_asset`, and pass channel references instead of making nx/ny/nz
copies where their lifetimes permit. Preserve detail normal maps, flips, alpha
and every blend mode.

**Acceptance:** equal decoded normal PNG pixels and mesh geometry for masked and
unmasked fixtures, flips, detail/no-detail maps; no normal-map working buffers
live during mesh serialization in allocation captures; repeatable heap/RSS
comparisons. Do not require an equal RSS decrease because allocators can retain
freed pages.
