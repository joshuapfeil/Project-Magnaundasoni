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
# Create archive
# Exclude generated directories that UBT recreates on first build.
# ---------------------------------------------------------------------------
(
    cd "$REPO_ROOT/unreal"
    zip -r "$DIST_DIR/${ARCHIVE_NAME}.zip" Plugin/ \
        --exclude "Plugin/Binaries/*" \
        --exclude "Plugin/Intermediate/*" \
        --exclude "Plugin/Saved/*" \
        --exclude "Plugin/DerivedDataCache/*" \
        --exclude "Plugin/.vs/*"
)

echo "==> Archive created: $DIST_DIR/${ARCHIVE_NAME}.zip"
