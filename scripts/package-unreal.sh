#!/usr/bin/env sh
# package-unreal.sh — Package the Magnaundasoni Unreal Engine plugin for
# distribution.
#
# Usage:
#   scripts/package-unreal.sh [--version <ver>]
#
# Options:
#   --version   Semver string to embed in the archive name (default: read from
#               unreal/Plugin/Magnaundasoni.uplugin, or "dev").
#
# Outputs:
#   dist/unreal/magnaundasoni-unreal-<version>.zip
#
# Description:
#   Creates a distributable ZIP archive of the Magnaundasoni Unreal plugin
#   located in unreal/Plugin/.  The archive does NOT include generated
#   Binaries/, Intermediate/, or Saved/ directories — those are produced by
#   Unreal Build Tool (UBT) on the developer's machine after installing the
#   plugin.
#
#   Pre-built native binaries that belong in the plugin's Binaries/ directory
#   should be placed there before running this script.  See docs/RELEASE_PROCESS.md
#   for the expected layout.
#
# Notes:
#   • Run from the repository root.
#   • Requires: zip (POSIX).

set -eu

VERSION=""

while [ $# -gt 0 ]; do
    case "$1" in
        --version) VERSION="$2"; shift 2 ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

# ---------------------------------------------------------------------------
# Resolve version from .uplugin when not supplied
# ---------------------------------------------------------------------------
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UPLUGIN="$REPO_ROOT/unreal/Plugin/Magnaundasoni.uplugin"

if [ -z "$VERSION" ]; then
    if [ -f "$UPLUGIN" ]; then
        VERSION=$(grep '"VersionName"' "$UPLUGIN" | head -1 \
            | sed 's/.*"VersionName"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/')
    fi
    VERSION="${VERSION:-dev}"
fi

ARCHIVE_NAME="magnaundasoni-unreal-${VERSION}"
PLUGIN_DIR="$REPO_ROOT/unreal/Plugin"
DIST_DIR="$REPO_ROOT/dist/unreal"

echo "==> Packaging Unreal plugin"
echo "    version  : $VERSION"
echo "    source   : $PLUGIN_DIR"
echo "    output   : $DIST_DIR/${ARCHIVE_NAME}.zip"

mkdir -p "$DIST_DIR"

# ---------------------------------------------------------------------------
# Validate source directory
# ---------------------------------------------------------------------------
if [ ! -d "$PLUGIN_DIR" ]; then
    echo "ERROR: Unreal plugin directory not found: $PLUGIN_DIR" >&2
    exit 1
fi

if [ ! -f "$UPLUGIN" ]; then
    echo "ERROR: .uplugin descriptor not found at: $UPLUGIN" >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Stage native headers alongside plugin source so the package is
# self-contained when dropped into a project's Plugins/ directory.
# The ThirdParty tree is added temporarily and cleaned up afterwards.
# ---------------------------------------------------------------------------
NATIVE_THIRDPARTY="$PLUGIN_DIR/Source/ThirdParty/Magnaundasoni"
NATIVE_INCLUDE_SRC="$REPO_ROOT/native/include"

if [ -d "$NATIVE_INCLUDE_SRC" ]; then
    mkdir -p "$NATIVE_THIRDPARTY"
    cp -r "$NATIVE_INCLUDE_SRC" "$NATIVE_THIRDPARTY/include"
    CLEANUP_THIRDPARTY=1
else
    echo "WARNING: native/include not found; headers will not be bundled." >&2
    CLEANUP_THIRDPARTY=0
fi

# ---------------------------------------------------------------------------
# Create archive
# Exclude generated directories that UBT recreates on first build.
# ---------------------------------------------------------------------------
(
    cd "$REPO_ROOT/unreal"
    zip -r "$DIST_DIR/${ARCHIVE_NAME}.zip" Plugin/ \
        -x "Plugin/Binaries/*" \
        -x "Plugin/Intermediate/*" \
        -x "Plugin/Saved/*" \
        -x "Plugin/DerivedDataCache/*" \
        -x "Plugin/.vs/*"
)

# Clean up the temporarily staged ThirdParty headers
if [ "${CLEANUP_THIRDPARTY:-0}" -eq 1 ]; then
    rm -rf "$NATIVE_THIRDPARTY"
    # Remove the ThirdParty parent only if it is now empty
    THIRDPARTY_PARENT="$(dirname "$NATIVE_THIRDPARTY")"
    if [ -d "$THIRDPARTY_PARENT" ] && [ -z "$(ls -A "$THIRDPARTY_PARENT")" ]; then
        rm -rf "$THIRDPARTY_PARENT"
    fi
fi

echo "==> Archive created: $DIST_DIR/${ARCHIVE_NAME}.zip"
