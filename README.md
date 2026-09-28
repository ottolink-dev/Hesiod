# Hesiod

A desktop application for node-based procedural terrain generation.
- binaries: https://sourceforge.net/projects/hesiod
- Discord: https://discord.gg/HBgFCjdV8W
- tutorial videos (WIP): https://www.youtube.com/playlist?list=PLvla2FXp5tDxbPypf_Mp66gWzX_Lga3DK
- documentation (WIP): https://hesioddoc.readthedocs.io
- roadmap: https://github.com/otto-link/Hesiod/discussions/608

>[!WARNING] 
> Work in progress, use at your own risk!

<img width="1918" height="1031" alt="Screenshot_2026-04-14_22-22-51" src="https://github.com/user-attachments/assets/d891c078-a385-467a-9fb6-7c6565bf29c2" />

<img width="1922" height="1080" alt="Screenshot_2025-12-06_14-35-41" src="https://github.com/user-attachments/assets/46d66c8e-752b-4a83-b1e5-4b309820ed70" />

## License

This project is licensed under the GNU General Public License v3.0.

## Getting started

### Standalone executables for Windows and Linux

Available here: https://sourceforge.net/projects/hesiod

### Nix / NixOS

The repository is a Nix flake. To try Hesiod without installing anything:

``` bash
nix run 'git+https://github.com/ottolink-dev/Hesiod?ref=main'   # main branch
nix run 'git+https://github.com/ottolink-dev/Hesiod?ref=dev'    # development branch
```

Use the `git+https://` form: the `github:` shorthand fetches a tarball that
does not include the submodules, and the flake refuses to build such a tree.

To install it on NixOS, add the flake as an input and put
`hesiod.packages.${system}.default` in `environment.systemPackages`
(an overlay is exported as `overlays.default` too). GPU acceleration needs
an OpenCL ICD on the host, for instance `hardware.graphics.extraPackages`
with your vendor's OpenCL package on NixOS.

The launcher runs Hesiod from `~/.local/share/hesiod` (honouring
`XDG_DATA_HOME`), where the packaged `data/` directory is symlinked and
run-time files such as the texture cache are written.

### Windows

#### Getting the sources

Use `PowerShell` to clone the repository (because Visual Studio won't clone the submodules) using `git` command lines:
``` bash
git clone --recurse-submodules git@github.com:otto-link/Hesiod.git
```

Submodules are fetched over https; no GitHub account is required to build.

Install [Qt6](https://doc.qt.io/qt-6/windows.html) and install the missing OpenSource dependencies using `vcpkg`:
```
vcpkg install glfw3 opengl gsl glew freeglut glm libpng glm opencl assimp spdlog opencv[openexr]
```

You should then be able to build the sources using Visual Studio.

### Linux

There are also required external dependencies for ubuntu you can execute:
```
sudo apt-get -y install libglfw3 libglfw3-dev libglew-dev libopengl-dev freeglut3-dev libboost-all-dev libeigen3-dev libglm-dev fuse libfuse2 ocl-icd-opencl-dev libassimp-dev qt6-webengine-dev zlib1g-dev
```

#### Getting the sources

Use `git` to retrieve the sources: 
``` bash
git clone git@github.com:otto-link/Hesiod.git
cd Hesiod
git submodule update --init --recursive
```

Submodules are fetched over https; no GitHub account is required to build.

#### Building

Build by making a build directory (i.e. `build/`), run `cmake` in that dir, and then use `make` to build the desired target.

Example:
``` bash
mkdir build && cd build
cmake ..
make hesiod
```

Start the main executable:
```
bin/./hesiod
```

## Dependencies and module structures

```mermaid
---
config:
  layout: dagre
title: Hesiod modules
---
flowchart TD
 subgraph subGraph0["Node Editor"]
        GNode["GNode"]
        GNodeGUI["GNodeGUI"]
  end
 subgraph subGraph1["Core Algorithms"]
        CLWrapper["CLWrapper"]
        HighMap["HighMap"]
        PointSampler["PointSampler"]
        CLErrorLookup["CLErrorLookup"]
        OpenCL["OpenCL"]
        FastNoiseLite["FastNoiseLite"]
  end
    Hesiod["Hesiod"] --> HighMap & GNode & GNodeGUI & Meta["Meta"] & QTerrainRenderer["QTerrainRenderer"] & QTextureDownloader["QTextureDownloader"]
    HighMap --> CLWrapper & PointSampler & FastNoiseLite
    CLWrapper --> CLErrorLookup & OpenCL
    QTerrainRenderer --> OpenGL["OpenGL"] & ImGui["ImGui"]
     OpenCL:::non_hesiod
     FastNoiseLite:::non_hesiod
     OpenGL:::non_hesiod
     ImGui:::non_hesiod
    classDef non_hesiod fill:#f4f4f4,stroke:#999,color:#555,font-size:12px
```
