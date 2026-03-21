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
#   located in unreal/Plugin/. The archive does NOT include generated
#   Binaries/, Intermediate/, or Saved/ directories — those are produced by
#   Unreal Build Tool (UBT) on the developer's machine after installing the
#   plugin.
#
#   If dist/native/*.zip archives are present (for example the release workflow's
#   native artifacts), the script stages their pre-built libraries into
#   Plugin/Source/ThirdParty/Magnaundasoni/<Platform>/ so the packaged plugin can
#   be dropped straight into a UE project's Plugins/ directory with no game
#   Build.cs edits or extra native ZIP download.
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
NATIVE_DIST_DIR="$REPO_ROOT/dist/native"
STAGE_DIR="$(mktemp -d "${TMPDIR:-/tmp}/magnaundasoni-unreal.XXXXXX")"
STAGED_PLUGIN_DIR="$STAGE_DIR/Plugin"

echo "==> Packaging Unreal plugin"
echo "    version  : $VERSION"
echo "    source   : $PLUGIN_DIR"
echo "    output   : $DIST_DIR/${ARCHIVE_NAME}.zip"

cleanup() {
    rm -rf "$STAGE_DIR"
}

trap cleanup EXIT INT TERM

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

cp -R "$PLUGIN_DIR" "$STAGED_PLUGIN_DIR"

# ---------------------------------------------------------------------------
# Stage native headers alongside plugin source so the package is
# self-contained when dropped into a project's Plugins/ directory.
# The ThirdParty tree is added temporarily and cleaned up afterwards.
# ---------------------------------------------------------------------------
NATIVE_THIRDPARTY="$STAGED_PLUGIN_DIR/Source/ThirdParty/Magnaundasoni"
NATIVE_INCLUDE_SRC="$REPO_ROOT/native/include"

if [ -d "$NATIVE_INCLUDE_SRC" ]; then
    mkdir -p "$NATIVE_THIRDPARTY"
    cp -r "$NATIVE_INCLUDE_SRC" "$NATIVE_THIRDPARTY/include"
else
    echo "WARNING: native/include not found; headers will not be bundled." >&2
fi

# ---------------------------------------------------------------------------
# Bundle pre-built native libraries when native release archives are available.
# This keeps the packaged plugin self-contained while still allowing the script
# to run locally without platform archives.
# ---------------------------------------------------------------------------
bundle_native_zip() {
    if ! python3 - "$1" "$NATIVE_THIRDPARTY" <<'PY'
import os
import shutil
import sys
import zipfile

zip_path, output_root = sys.argv[1:3]
archive_name = os.path.basename(zip_path).lower()

if "native-windows" in archive_name:
    platform_dir = "Win64"
    files_to_copy = {
        "bin/magnaundasoni.dll": "magnaundasoni.dll",
        "lib/magnaundasoni.lib": "magnaundasoni.lib",
    }
elif "native-linux" in archive_name:
    platform_dir = "Linux"
    files_to_copy = {
        "lib/libmagnaundasoni.so": "libmagnaundasoni.so",
    }
elif "native-macos" in archive_name:
    platform_dir = "Mac"
    files_to_copy = {
        "lib/libmagnaundasoni.dylib": "libmagnaundasoni.dylib",
    }
else:
    print(
        f"WARNING: Skipping unrecognized native archive path: {zip_path}",
        file=sys.stderr,
    )
    sys.exit(0)

with zipfile.ZipFile(zip_path) as archive:
    missing = []
    for source_rel_path, target_name in files_to_copy.items():
        try:
            with archive.open(source_rel_path) as src:
                target_dir = os.path.join(output_root, platform_dir)
                os.makedirs(target_dir, exist_ok=True)
                with open(os.path.join(target_dir, target_name), "wb") as dst:
                    shutil.copyfileobj(src, dst)
        except KeyError:
            missing.append(source_rel_path)

    if missing:
        print(
            f"ERROR: {zip_path} is missing expected file(s): {', '.join(missing)}",
            file=sys.stderr,
        )
        sys.exit(1)
PY
    then
        echo "ERROR: Failed to bundle native archive: $1" >&2
        exit 1
    fi
}

HAS_NATIVE_ARCHIVES=0
if [ -d "$NATIVE_DIST_DIR" ]; then
    set -- "$NATIVE_DIST_DIR"/*.zip
    if [ -e "$1" ]; then
        for native_zip in "$@"; do
            HAS_NATIVE_ARCHIVES=1
            echo "    bundling : $native_zip"
            bundle_native_zip "$native_zip"
        done
    fi
fi

if [ "$HAS_NATIVE_ARCHIVES" -eq 0 ]; then
    echo "WARNING: No dist/native/*.zip archives found; packaging source + headers only." >&2
fi

# Mark the staged descriptor as an installed plugin so the packaged ZIP behaves
# like a drop-in distribution rather than an in-repo development copy.
if ! python3 - "$STAGED_PLUGIN_DIR/Magnaundasoni.uplugin" <<'PY'
import json
import sys

descriptor_path = sys.argv[1]

try:
    with open(descriptor_path, "r", encoding="utf-8") as descriptor_file:
        descriptor = json.load(descriptor_file)
except FileNotFoundError:
    print(f"ERROR: Plugin descriptor not found: {descriptor_path}", file=sys.stderr)
    sys.exit(1)
except json.JSONDecodeError as exc:
    print(
        f"ERROR: Plugin descriptor is not valid JSON: {descriptor_path}: {exc}",
        file=sys.stderr,
    )
    sys.exit(1)

descriptor["Installed"] = True

with open(descriptor_path, "w", encoding="utf-8") as descriptor_file:
    json.dump(descriptor, descriptor_file, indent=4)
    descriptor_file.write("\n")
PY
then
    echo "ERROR: Failed to mark staged plugin descriptor as Installed=true" >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Create archive
# Exclude generated directories that UBT recreates on first build.
# ---------------------------------------------------------------------------
(
    cd "$STAGE_DIR"
    zip -r "$DIST_DIR/${ARCHIVE_NAME}.zip" Plugin/ \
        -x "Plugin/Binaries/*" \
        -x "Plugin/Intermediate/*" \
        -x "Plugin/Saved/*" \
        -x "Plugin/DerivedDataCache/*" \
        -x "Plugin/.vs/*"
)

echo "==> Archive created: $DIST_DIR/${ARCHIVE_NAME}.zip"
