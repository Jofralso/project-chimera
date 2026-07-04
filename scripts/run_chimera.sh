#!/usr/bin/env bash
set -euo pipefail

# Simple helper to configure, build, test and run Project Chimera

PROJ_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$PROJ_ROOT/build}"
JOBS="${JOBS:-$(nproc)}"
CMAKE_TYPE="${CMAKE_TYPE:-Release}"

usage() {
  cat <<EOF
Usage: $(basename "$0") <command> [-- <args>]

Commands:
  build           Configure and build (cmake -S . -B build)
  test            Run ctest in build dir
  play [args...]  Build (if needed) and run chimera-play with given args
  desktop [args...]  Build (if needed) and run chimera-desktop with given args
  jetson [args...]   Build + run optimized for Jetson Orin Nano
  all             build + test
  help            Show this message

Examples:
  $(basename "$0") build
  $(basename "$0") play -- -d 5 --alsa
  $(basename "$0") desktop -- --alsa --scale 2
EOF
}

cmake_config() {
  mkdir -p "$BUILD_DIR"
  cmake -S "$PROJ_ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CMAKE_TYPE"
}

cmake_build() {
  cmake --build "$BUILD_DIR" -- -j"$JOBS"
}

run_ctest() {
  if [ ! -d "$BUILD_DIR" ]; then
    echo "Build directory not found, run '$0 build' first." >&2
    return 1
  fi
  ctest --test-dir "$BUILD_DIR" --output-on-failure
}

run_play() {
  local bin="$BUILD_DIR/software/apps/chimera-play"
  if [ ! -x "$bin" ]; then
    echo "Binary not found, building..."
    cmake_config
    cmake_build
  fi
  exec "$bin" "$@"
}

run_desktop() {
  local bin="$BUILD_DIR/software/apps/chimera-desktop"
  if [ ! -x "$bin" ]; then
    echo "Binary not found, building..."
    cmake_config
    cmake_build
  fi
  exec "$bin" "$@"
}

run_jetson() {
  echo "Building for Jetson Orin Nano..."
  local jetson_dir="${BUILD_DIR}-jetson"
  mkdir -p "$jetson_dir"
  if [ ! -f "$jetson_dir/Makefile" ]; then
    cmake -S "$PROJ_ROOT" -B "$jetson_dir" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCHIMERA_JETSON=ON \
      -DCMAKE_TOOLCHAIN_FILE="$PROJ_ROOT/cmake/JetsonToolchain.cmake" \
      "$@"
  fi
  cmake --build "$jetson_dir" -- -j"$(nproc)"
  echo ""
  echo "Jetson build complete: $jetson_dir/software/apps/chimera-desktop"
  echo ""
  echo "To run on device:"
  echo "  export SDL_VIDEODRIVER=kmsdrm"
  echo "  $jetson_dir/software/apps/chimera-desktop --touch --alsa"
}

if [ $# -lt 1 ]; then
  usage
  exit 1
fi

cmd="$1"
shift || true

case "$cmd" in
  build)
    cmake_config
    cmake_build
    ;;
  test)
    run_ctest
    ;;
  play)
    # pass remaining args to the binary
    run_play "$@"
    ;;
  desktop)
    run_desktop "$@"
    ;;
  jetson)
    run_jetson "$@"
    ;;
  all)
    cmake_config
    cmake_build
    run_ctest
    ;;
  help|-h|--help)
    usage
    ;;
  *)
    echo "Unknown command: $cmd" >&2
    usage
    exit 2
    ;;
esac
