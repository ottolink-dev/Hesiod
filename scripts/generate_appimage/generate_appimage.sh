#!/bin/bash
set -e

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
appimage_dir="${1:-$script_dir}"
tools_dir="${2:-$script_dir}"
root_dir="${3:-$(cd "$script_dir/../.." && pwd)}"

# always use build/bin for binary directory
bin_dir="${root_dir}/build/bin"
hesiod_bin="${bin_dir}/hesiod"

echo "[generate_appimage] Root directory: ${root_dir}"
echo "[generate_appimage] AppImage directory: ${appimage_dir}"
echo "[generate_appimage] Tools directory: ${tools_dir}"
echo "[generate_appimage] Bin directory: ${bin_dir}"
echo "[generate_appimage] Hesiod executable: ${hesiod_bin}"

if [ ! -f "${hesiod_bin}" ]; then
    echo "[generate_appimage] Error: hesiod executable not found at ${hesiod_bin}"
    exit 1
fi

# --- Setup Qt Environment

echo "[generate_appimage] Detecting Qt6 configuration..."
if [ -n "${Qt6_ROOT}" ]; then
    echo "[generate_appimage] Qt6_ROOT is set to: ${Qt6_ROOT}"
    if [ -x "${Qt6_ROOT}/bin/qmake" ]; then
        export QMAKE="${Qt6_ROOT}/bin/qmake"
    elif [ -x "${Qt6_ROOT}/bin/qmake6" ]; then
        export QMAKE="${Qt6_ROOT}/bin/qmake6"
    fi
elif [ -n "${QT_ROOT}" ]; then
    echo "[generate_appimage] QT_ROOT is set to: ${QT_ROOT}"
    if [ -x "${QT_ROOT}/bin/qmake" ]; then
        export QMAKE="${QT_ROOT}/bin/qmake"
    elif [ -x "${QT_ROOT}/bin/qmake6" ]; then
        export QMAKE="${QT_ROOT}/bin/qmake6"
    fi
fi

if [ -z "${QMAKE}" ]; then
    if command -v qmake6 &> /dev/null; then
        export QMAKE="$(which qmake6)"
        echo "[generate_appimage] Found qmake6 via PATH: ${QMAKE}"
    elif command -v qmake &> /dev/null; then
        export QMAKE="$(which qmake)"
        echo "[generate_appimage] Found qmake via PATH: ${QMAKE}"
    fi
fi

if [ -n "${QMAKE}" ]; then
    export PATH="$(dirname "${QMAKE}"):${PATH}"
    echo "[generate_appimage] Using QMAKE: ${QMAKE}"
    qt_version="$("${QMAKE}" -query QT_VERSION 2>/dev/null || echo "unknown")"
    echo "[generate_appimage] Detected Qt version: ${qt_version}"
else
    echo "[generate_appimage] WARNING: qmake/qmake6 not found! linuxdeploy-plugin-qt might fail."
fi

cd "${appimage_dir}"
echo "[generate_appimage] Cleaning old AppDir..."
rm -rf AppDir

# --- Download Tools

echo "[generate_appimage] Verifying linuxdeploy tools in ${tools_dir}..."
wget -nc https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage -P "${tools_dir}"
wget -nc https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage -P "${tools_dir}"

chmod 0744 "${tools_dir}/linuxdeploy-x86_64.AppImage"
chmod 0744 "${tools_dir}/linuxdeploy-plugin-qt-x86_64.AppImage"

# --- Deploy AppImage

echo "[generate_appimage] Running linuxdeploy and linuxdeploy-plugin-qt..."
"${tools_dir}/linuxdeploy-x86_64.AppImage" \
    --appdir AppDir \
    --executable "${hesiod_bin}" \
    --plugin qt \
    --output appimage \
    -d "${appimage_dir}/hesiod.desktop" \
    -i "${appimage_dir}/icon_hesiod.png"

# --- Package AppImage

echo "[generate_appimage] Copying generated AppImage to ${bin_dir}/hesiod.AppImage..."
cp hesiod-x86_64.AppImage "${bin_dir}/hesiod.AppImage"

# add the data folder and create a zip
echo "[generate_appimage] Creating zip package with AppImage and data folder..."
cd "${bin_dir}"
zip -r hesiod.AppImage.zip hesiod.AppImage data

echo "[generate_appimage] AppImage generation complete: ${bin_dir}/hesiod.AppImage.zip"


