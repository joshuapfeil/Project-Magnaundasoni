#!/usr/bin/env sh
# package-unity.sh — Package the Magnaundasoni Unity plugin for distribution.
#
# Usage:
#   scripts/package-unity.sh [--version <ver>]
#
# Options:
#   --version   Semver string to embed in the archive name (default: read from
#               unity/plugin/package.json, or "dev").
#
# Outputs:
#   dist/unity/magnaundasoni-unity-<version>.zip
#
# Description:
#   This script creates a distributable ZIP archive of the Unity Package
#   Manager (UPM) plugin located in unity/plugin/.  It does NOT require the
#   Unity Editor to be installed.
#
#   Native library binaries (.dll / .so / .dylib) that are placed into
#   unity/plugin/Runtime/Plugins/ by the native packaging step (see
#   package-native.sh) will be included automatically if present.
#
#   The resulting archive is suitable for:
#     • UPM installation via "Add package from tarball" after first extracting
#       this .zip and re-packaging its contents as a .tgz (tar.gz) with
#       package.json at the archive root, or
#     • manual extraction into a project's Packages/ folder.
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
# Resolve version from package.json when not supplied
# ---------------------------------------------------------------------------
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PACKAGE_JSON="$REPO_ROOT/unity/plugin/package.json"

if [ -z "$VERSION" ]; then
    if [ -f "$PACKAGE_JSON" ]; then
        # Portable extraction without jq: grab the first "version" field value
        VERSION=$(grep '"version"' "$PACKAGE_JSON" | head -1 \
            | sed 's/.*"version"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/')
    fi
    VERSION="${VERSION:-dev}"
fi

ARCHIVE_NAME="magnaundasoni-unity-${VERSION}"
PLUGIN_DIR="$REPO_ROOT/unity/plugin"
DIST_DIR="$REPO_ROOT/dist/unity"

echo "==> Packaging Unity plugin"
echo "    version  : $VERSION"
echo "    source   : $PLUGIN_DIR"
echo "    output   : $DIST_DIR/${ARCHIVE_NAME}.zip"

mkdir -p "$DIST_DIR"

# ---------------------------------------------------------------------------
# Validate source directory
# ---------------------------------------------------------------------------
if [ ! -d "$PLUGIN_DIR" ]; then
    echo "ERROR: Unity plugin directory not found: $PLUGIN_DIR" >&2
    exit 1
fi

if [ ! -f "$PACKAGE_JSON" ]; then
    echo "ERROR: package.json not found at: $PACKAGE_JSON" >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Create archive from the plugin directory contents
# The archive mirrors the UPM package layout: plugin/ becomes the root.
# ---------------------------------------------------------------------------
(
    cd "$REPO_ROOT/unity"
    zip -r "$DIST_DIR/${ARCHIVE_NAME}.zip" plugin/ \
        -x "plugin/*/Library/*" \
        -x "plugin/*/Temp/*" \
        -x "plugin/*/Obj/*" \
        -x "plugin/*/Build/*" \
        -x "plugin/*/Logs/*" \
        -x "plugin/*.sln" \
        -x "plugin/*.csproj"
)

echo "==> Archive created: $DIST_DIR/${ARCHIVE_NAME}.zip"
