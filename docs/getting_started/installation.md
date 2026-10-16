# Download and installation

Get Hesiod from the [official download page](https://hesiodengine.com/#download).
Select a channel, then download the package for your operating system:

- **Stable:** released versions.
- **Dev build:** newer features from the development branch. These builds can
  contain regressions; keep a backup of projects before opening them in a newer build.

The download page shows the version or build date for each available package.
Windows and Linux packages are available in both channels. An experimental
Apple Silicon package is available in **Dev build**. Development packages include
`SOURCE.txt` identifying the source revision (inside the app resources on macOS).

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

Select **Dev build → macOS** on the [download page](https://hesiodengine.com/#download).
The experimental package requires **Apple Silicon (M-series)** and **macOS 15 or
later**. It does not support Intel Macs. The ZIP contains a standalone app with
its runtime libraries; Homebrew is not required.

1. Extract the ZIP and move `Hesiod.app` to Applications.
2. Open the app. It is ad-hoc signed and is **not Apple-notarized**.
3. If macOS blocks this trusted download, open **System Settings → Privacy &
   Security** and use **Open Anyway**, then confirm the launch.

The CI build passed its ARM64, bundled-library and code-signature audits. Real-Mac
workload profiling remains separate; this is an experimental development build.
The source revision and dependency information are in
`Hesiod.app/Contents/Resources/SOURCE.txt`. Runtime files are stored under
`~/Library/Application Support/hesiod/runtime/`.

The [macOS CI run](https://github.com/ottolink-dev/Hesiod/actions/runs/38924866656)
also provides an Actions artifact with the app ZIP, SHA-256 checksum and
`verification.json`, which records the package's minimum macOS version.
Actions artifacts require a GitHub sign-in and expire after 14 days; the website
provides the published package without that requirement.

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
