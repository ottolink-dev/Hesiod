#!/usr/bin/env bash
set -e

# Default variables
BUILD_DIR="${1:-build}"
BUILD_TYPE="${2:-Debug}"
NUM_JOBS=8

echo "=================================================="
echo " Hesiod Build Profiler Runner"
echo "=================================================="
echo " Build directory : ${BUILD_DIR}"
echo " Build type      : ${BUILD_TYPE}"
echo " Parallel jobs   : ${NUM_JOBS}"
echo " Node set        : Minimal (HESIOD_MINIMAL_NODE_SET=ON)"
echo " PCH             : ON (HESIOD_ENABLE_PCH=ON)"
echo " Profiling       : ON"
echo "=================================================="
echo ""

# Confirmation prompt guard
read -r -p "This will clean and build '${BUILD_DIR}' with profiling enabled. Continue? [y/N] " response
case "$response" in
    [yY][eE][sS]|[yY])
        echo ""
        echo "Starting profiled build process..."
        ;;
    *)
        echo "Aborted by user."
        exit 0
        ;;
esac

# 1. Configure CMake with profiling
echo "--- [1/3] Configuring CMake..."
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
mkdir -p "${BUILD_DIR}"
ABS_BUILD_DIR="$(cd "${BUILD_DIR}" && pwd)"
export HESIOD_BUILD_TIME_LOG="${ABS_BUILD_DIR}/.hesiod_build_times.log"
rm -f "${HESIOD_BUILD_TIME_LOG}"
chmod +x "${SCRIPT_DIR}/timed_compile.sh"

cmake -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DHESIOD_PROFILE_BUILD=ON \
    -DHESIOD_MINIMAL_NODE_SET=ON \
    -DHESIOD_ENABLE_PCH=ON \
    -DCMAKE_CXX_COMPILER_LAUNCHER="${SCRIPT_DIR}/timed_compile.sh"

# 2. Clean build directory / targets
echo "--- [2/3] Cleaning previous build artifacts..."
cmake --build "${BUILD_DIR}" --target clean 2>/dev/null || true

# 3. Build project
echo "--- [3/3] Building target 'hesiod' (-j${NUM_JOBS})..."
cmake --build "${BUILD_DIR}" --target hesiod -j "${NUM_JOBS}"

# 4. Run profiling aggregator
echo "--- Aggregating profiling data..."
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
python3 "${SCRIPT_DIR}/profile_build.py" "${BUILD_DIR}"
