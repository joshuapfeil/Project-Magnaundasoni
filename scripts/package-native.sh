#!/usr/bin/env sh
# package-native.sh — Build and package the native Magnaundasoni library for the
# current platform (Linux or macOS).
#
# Usage:
#   scripts/package-native.sh [--version <ver>] [--build-type <type>]
#
# Options:
#   --version     Semver string to embed in the archive name (default: from
#                 CMakeLists.txt or "dev").
#   --build-type  CMake build type: Release | RelWithDebInfo | Debug
#                 (default: Release)
#
# Outputs:
#   dist/native/magnaundasoni-native-<platform>-<arch>-<version>.zip
#
# Requirements:
#   cmake, a C++17-capable compiler, zip
#
# Notes:
#   • The script must be run from the repository root.
#   • The "build" directory under native/ is used for compilation and is
#     cleaned between runs to ensure a reproducible artifact.
#   • Set MAGNAUNDASONI_ENABLE_SIMD=OFF to disable SSE optimisations on
#     cross-compile targets that do not support them.

set -eu

# ---------------------------------------------------------------------------
# Defaults
# ---------------------------------------------------------------------------
VERSION=""
BUILD_TYPE="Release"
ENABLE_SIMD="${MAGNAUNDASONI_ENABLE_SIMD:-ON}"
ENABLE_RT="${MAGNAUNDASONI_ENABLE_RT:-OFF}"

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------
while [ $# -gt 0 ]; do
    case "$1" in
        --version)   VERSION="$2";     shift 2 ;;
        --build-type) BUILD_TYPE="$2"; shift 2 ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

# ---------------------------------------------------------------------------
# Detect version from CMakeLists.txt when not supplied
# ---------------------------------------------------------------------------
if [ -z "$VERSION" ]; then
    VERSION=$(grep -m1 "project(Magnaundasoni VERSION" native/CMakeLists.txt \
        | sed 's/.*VERSION[[:space:]]*\([0-9][0-9.]*\).*/\1/')
    VERSION="${VERSION:-dev}"
fi

# ---------------------------------------------------------------------------
# Platform / architecture detection
# ---------------------------------------------------------------------------
OS_NAME=$(uname -s | tr '[:upper:]' '[:lower:]')
ARCH=$(uname -m)

case "$OS_NAME" in
    linux*)  PLATFORM="linux" ;;
    darwin*) PLATFORM="macos" ;;
    *)       echo "Unsupported platform: $OS_NAME" >&2; exit 1 ;;
esac

case "$ARCH" in
    x86_64)  ARCH_TAG="x64" ;;
    aarch64|arm64) ARCH_TAG="arm64" ;;
    *)       ARCH_TAG="$ARCH" ;;
esac

ARCHIVE_NAME="magnaundasoni-native-${PLATFORM}-${ARCH_TAG}-${VERSION}"

# ---------------------------------------------------------------------------
# Directories
# ---------------------------------------------------------------------------
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NATIVE_DIR="$REPO_ROOT/native"
BUILD_DIR="$NATIVE_DIR/build"
STAGE_DIR="$REPO_ROOT/dist/native/stage-${PLATFORM}-${ARCH_TAG}"
DIST_DIR="$REPO_ROOT/dist/native"

echo "==> Packaging native library"
echo "    platform  : $PLATFORM-$ARCH_TAG"
echo "    version   : $VERSION"
echo "    build type: $BUILD_TYPE"
echo "    repo root : $REPO_ROOT"

# ---------------------------------------------------------------------------
# Clean and configure
# ---------------------------------------------------------------------------
rm -rf "$BUILD_DIR" "$STAGE_DIR"
mkdir -p "$BUILD_DIR" "$STAGE_DIR"

cmake -S "$NATIVE_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_INSTALL_PREFIX="$STAGE_DIR" \
    -DMAGNAUNDASONI_BUILD_TESTS=OFF \
    -DMAGNAUNDASONI_ENABLE_SIMD="$ENABLE_SIMD" \
    -DMAGNAUNDASONI_ENABLE_RT="$ENABLE_RT"

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" --parallel

# ---------------------------------------------------------------------------
# Install to staging area
# ---------------------------------------------------------------------------
cmake --install "$BUILD_DIR" --config "$BUILD_TYPE"

# ---------------------------------------------------------------------------
# Add a version manifest
# ---------------------------------------------------------------------------
cat > "$STAGE_DIR/VERSION" <<EOF
version=$VERSION
platform=$PLATFORM
arch=$ARCH_TAG
build_type=$BUILD_TYPE
built_at=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
EOF

# ---------------------------------------------------------------------------
# Zip
# ---------------------------------------------------------------------------
mkdir -p "$DIST_DIR"

(cd "$STAGE_DIR/.." && zip -r "$DIST_DIR/${ARCHIVE_NAME}.zip" "$(basename "$STAGE_DIR")/")

echo "==> Archive created: $DIST_DIR/${ARCHIVE_NAME}.zip"

# Clean staging directory
rm -rf "$STAGE_DIR"
