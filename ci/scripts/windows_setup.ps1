# ci/scripts/windows_setup.ps1
# Install build dependencies for Magnaundasoni on Windows CI runners.
# MSVC and CMake are pre-installed on GitHub-hosted windows-latest runners.

$ErrorActionPreference = "Stop"

Write-Host "==> Checking for required tools..."

# CMake
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "==> CMake not found; installing via winget..."
    winget install --id Kitware.CMake --silent --accept-package-agreements --accept-source-agreements
} else {
    $cmakeVer = (cmake --version | Select-Object -First 1)
    Write-Host "==> CMake found: $cmakeVer"
}

# Git (always available on GitHub-hosted runners)
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Write-Error "Git is required but not found."
}

Write-Host "==> Dependency check complete."
