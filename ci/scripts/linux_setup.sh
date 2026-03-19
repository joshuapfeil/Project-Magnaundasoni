#!/usr/bin/env bash
# ci/scripts/linux_setup.sh
# Install build dependencies for Magnaundasoni on Linux CI runners.
set -euo pipefail

echo "==> Updating package list..."
sudo apt-get update -qq

echo "==> Installing build dependencies..."
sudo apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    git \
    unzip \
    zip \
    wget

echo "==> Dependency installation complete."
