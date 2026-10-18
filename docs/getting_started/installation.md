# Download and installation

Get Hesiod from the [official download page](https://hesiodengine.com/#download).
Select a channel, then download the package for your operating system:

- **Stable:** released versions.
- **Dev build:** newer features from the development branch. These builds can
  contain regressions; keep a backup of projects before opening them in a newer build.

The download page shows the version or build date for each available package.
Windows and Linux packages are available in both channels. Development builds
include a `SOURCE.txt` file identifying the source revision.

## Windows

1. Download the Windows ZIP and extract the entire archive into a writable folder.
2. Open that folder and run `hesiod.exe`.
3. Keep the `data` folder, DLLs, Qt plugins and other bundled files together.
   Moving just the EXE or running it from inside the ZIP will not give it its
   required resources.

The current development package is for **64-bit x86 Windows**. Install your
GPU vendor's graphics driver, including its OpenCL runtime. The packaged DLLs
do not replace the device driver. You do not need the source-build tools to
run the downloaded package.

## Linux

Download and extract the Linux ZIP. From the extracted folder, run:

```bash
chmod +x hesiod.AppImage
./hesiod.AppImage
```

Keep the supplied `data` folder next to the AppImage and launch from that
directory so Hesiod can find its icons, gradients, examples and other resources.
You can also enable **Allow executing file as program** in your file manager.

The current development package targets **Linux x86-64** and is built on
**Ubuntu 24.04**. It needs a compatible system C library; packaging as an
AppImage does not make it compatible with every older distribution. Graphics
and OpenCL drivers come from your system.

If startup reports a missing FUSE library, install your distribution's FUSE 2
compatibility package (`libfuse2t64` on Ubuntu 24.04), or try extraction mode:

```bash
APPIMAGE_EXTRACT_AND_RUN=1 ./hesiod.AppImage
```

If startup reports that no OpenCL platform or device is available, check that
an OpenCL runtime for your hardware is installed. Setting Qt to offscreen mode
for a batch run does not install an OpenCL runtime.

### Nix / NixOS

The repository also provides a Nix flake. See the
[README's Nix instructions](https://github.com/ottolink-dev/Hesiod#nix--nixos)
for running or installing it and configuring host OpenCL support.

## macOS / Apple Silicon

Apple Silicon builds and standalone `.app` packaging are experimental and are
being validated in [PR #835](https://github.com/ottolink-dev/Hesiod/pull/835).
There is no macOS download on the website yet. Intel Mac support is not part
of that build.

A successful [macOS CI run](https://github.com/ottolink-dev/Hesiod/actions/workflows/macos-build.yml)
uploads a `hesiod-macos-arm64` artifact containing the app ZIP, a checksum,
source information and a verification report. These are temporary test
artifacts, retained for 14 days; downloading Actions artifacts requires a
GitHub sign-in.

For a successfully packaged build, extract the app ZIP and move `Hesiod.app`
to Applications. Use the minimum macOS version recorded in that build's
`verification.json`; it depends on the bundled libraries. Test packages are
ad-hoc signed and are not Apple-notarized. If macOS blocks a trusted build,
use **System Settings → Privacy & Security** to approve it.

## First launch and batch mode

Follow [Your first terrain](index.md) to create and export a small graph.
For running an existing project without the GUI, see
[Automation and batch mode](../guides/batch-headless.md). Batch rendering still
needs the packaged resources and runtime dependencies, plus export settings
in the graph if you want files written to disk.

## Building from source

Source-build prerequisites are separate from the requirements for downloaded
packages. Follow the [repository README](https://github.com/ottolink-dev/Hesiod#readme)
for build instructions. Development work targets `dev`; `main` contains the
stable source. Clone recursively so the required submodules are included.
