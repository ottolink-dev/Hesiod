#!/usr/bin/env bash
set -euo pipefail
source_dir=$(realpath "$1")
mkdir -p "$2"
out_dir=$(realpath "$2")
tools_dir=$(mktemp -d)
trap 'rm -rf "$tools_dir"' EXIT
cd "$tools_dir"
# Immutable release asset IDs with checksums; fail closed if upstream removes them.
gh api repos/linuxdeploy/linuxdeploy/releases/assets/602438599 -H 'Accept: application/octet-stream' > linuxdeploy-x86_64.AppImage
gh api repos/linuxdeploy/linuxdeploy-plugin-qt/releases/assets/525032210 -H 'Accept: application/octet-stream' > linuxdeploy-plugin-qt-x86_64.AppImage
sha256sum --check <<'CHECKSUMS'
8aea8da0f7f7039d2a2cecb14657d752a222a5e1d3825caeef186c82f751cdd1  linuxdeploy-x86_64.AppImage
cfc1055b2b9dbc08412b579f20990b7b41a17b61beaa5847dc9477c96c9e9617  linuxdeploy-plugin-qt-x86_64.AppImage
CHECKSUMS
chmod +x ./*.AppImage
export APPIMAGE_EXTRACT_AND_RUN=1
export QMAKE="${QT_ROOT_DIR:?Qt installation missing}/bin/qmake"
export PATH="$tools_dir:$PATH"
./linuxdeploy-x86_64.AppImage --appdir AppDir \
  --executable "$source_dir/build/bin/hesiod" --plugin qt --output appimage \
  --desktop-file "$source_dir/scripts/generate_appimage/hesiod.desktop" \
  --icon-file "$source_dir/scripts/generate_appimage/icon_hesiod.png"
mkdir package
cp hesiod-x86_64.AppImage package/hesiod.AppImage
cp "$source_dir/LICENSE" package/LICENSE
cp -a "$source_dir/build/bin/data" package/data
printf '%s\n' "${SOURCE_SHA:?Source revision missing}" > package/SOURCE.txt
# Test that the packaged executable and its libraries can be loaded.
(cd package && QT_QPA_PLATFORM=offscreen ./hesiod.AppImage --help)
(cd package && zip -qr "$out_dir/hesiod-linux-x86_64.zip" .)
