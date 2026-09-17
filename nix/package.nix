# Hesiod package. Called from flake.nix (and the overlay) with the flake source
# tree, which must include the submodules, and the computed version string.
{
  lib,
  stdenv,
  src,
  version,
  cmake,
  ninja,
  pkg-config,
  qt6,
  spdlog,
  nlohmann_json,
  gsl,
  assimp,
  glm,
  opencv,
  glfw,
  glew,
  freeglut,
  libGL,
  libGLU,
  boost,
  eigen,
  libpng,
  zlib,
  opencl-headers,
  opencl-clhpp,
  ocl-icd,
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "hesiod";
  inherit version src;

  nativeBuildInputs = [
    cmake
    ninja
    pkg-config
    qt6.wrapQtAppsHook
  ];

  buildInputs = [
    qt6.qtbase
    qt6.qtwayland
    qt6.qtsvg
    qt6.qttools
    qt6.qtwebengine
    spdlog
    nlohmann_json
    gsl
    assimp
    glm
    opencv
    glfw
    glew
    freeglut
    libGL
    libGLU
    boost
    eigen
    libpng
    zlib
    opencl-headers
    opencl-clhpp
    ocl-icd
  ];

  cmakeFlags = [
    (lib.cmakeBool "HESIOD_ENABLE_GENERATE_APP_IMAGE" false)
    # HighMap's benchmarks FetchContent google/benchmark at configure time;
    # the sandbox has no network and Hesiod never runs them.
    (lib.cmakeBool "HIGHMAP_ENABLE_BENCHMARKS" false)
  ];

  # HighMap vendors nn-c's Triangle library, which is K&R C. GCC 15 compiles C
  # as C23 by default, where an empty () prototype means (void) and the
  # 4-argument `void triangulate()` definition fails. cmake seeds
  # CMAKE_C_FLAGS from $CFLAGS, so this reaches C only; C++ is untouched.
  env.CFLAGS = "-std=gnu17";

  # The tree has no install() rules: the build tree copies data/ next to the
  # binary and the AppImage script does the rest. Lay it out by hand and wrap
  # the real binary ourselves (see hesiod-wrapper.sh for why).
  dontWrapQtApps = true;

  installPhase = ''
    runHook preInstall

    install -Dm755 bin/hesiod $out/libexec/hesiod/hesiod

    mkdir -p $out/share/hesiod
    cp -r $src/Hesiod/data $out/share/hesiod/data
    chmod -R u+w $out/share/hesiod/data
    # The build writes `git describe` output here; the sandbox has no .git.
    echo "v${finalAttrs.version}" > $out/share/hesiod/data/git_version.txt

    # Blender bridge add-on, ready to install from Blender's preferences.
    install -Dm644 $src/bridges/Blender/hesiod_streamer.zip \
      $out/share/hesiod/blender/hesiod_streamer.zip

    install -Dm644 $src/Hesiod/data/hesiod_icon.png \
      $out/share/icons/hicolor/512x512/apps/hesiod.png
    install -Dm644 ${./hesiod.desktop} $out/share/applications/hesiod.desktop

    runHook postInstall
  '';

  postFixup = ''
    wrapQtApp $out/libexec/hesiod/hesiod
    mkdir -p $out/bin
    substitute ${./hesiod-wrapper.sh} $out/bin/hesiod \
      --subst-var out \
      --subst-var-by shell ${stdenv.shell}
    chmod 755 $out/bin/hesiod
  '';

  meta = {
    description = "Node-based procedural terrain generator";
    homepage = "https://github.com/ottolink-dev/Hesiod";
    license = lib.licenses.gpl3Only;
    platforms = lib.platforms.linux;
    mainProgram = "hesiod";
  };
})
