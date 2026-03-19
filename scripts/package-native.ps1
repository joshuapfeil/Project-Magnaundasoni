<#
.SYNOPSIS
    Build and package the native Magnaundasoni library for Windows (x64).

.DESCRIPTION
    Configures and builds the CMake project in Release mode, installs the
    artifacts into a staging directory, and creates a ZIP archive in
    dist\native\.

.PARAMETER Version
    Semver string to embed in the archive name. Defaults to the version read
    from native\CMakeLists.txt, or "dev" as a fallback.

.PARAMETER BuildType
    CMake build type. Defaults to "Release".

.PARAMETER EnableSimd
    Whether to pass MAGNAUNDASONI_ENABLE_SIMD=ON to CMake. Defaults to ON.

.PARAMETER EnableRt
    Whether to pass MAGNAUNDASONI_ENABLE_RT=ON to CMake. Defaults to OFF.

.EXAMPLE
    .\scripts\package-native.ps1
    .\scripts\package-native.ps1 -Version 1.2.3 -BuildType RelWithDebInfo
#>

[CmdletBinding()]
param(
    [string] $Version   = "",
    [string] $BuildType = "Release",
    [string] $EnableSimd = "ON",
    [string] $EnableRt   = "OFF"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ---------------------------------------------------------------------------
# Resolve repository root (one level above the scripts\ directory)
# ---------------------------------------------------------------------------
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot  = Split-Path -Parent $ScriptDir
$NativeDir = Join-Path $RepoRoot "native"

# ---------------------------------------------------------------------------
# Detect version from CMakeLists.txt when not supplied
# ---------------------------------------------------------------------------
if ([string]::IsNullOrWhiteSpace($Version)) {
    $CmakeLists = Join-Path $NativeDir "CMakeLists.txt"
    $match = Select-String -Path $CmakeLists -Pattern 'project\(Magnaundasoni VERSION\s+([0-9][0-9.]*)' |
             Select-Object -First 1
    if ($match) {
        $Version = $match.Matches[0].Groups[1].Value
    } else {
        $Version = "dev"
    }
}

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
$ArchiveName = "magnaundasoni-native-windows-x64-$Version"
$BuildDir    = Join-Path $NativeDir "build"
$StageDir    = Join-Path $RepoRoot "dist\native\stage-windows-x64"
$DistDir     = Join-Path $RepoRoot "dist\native"

Write-Host "==> Packaging native library (Windows)"
Write-Host "    version   : $Version"
Write-Host "    build type: $BuildType"
Write-Host "    repo root : $RepoRoot"

# ---------------------------------------------------------------------------
# Clean and reconfigure
# ---------------------------------------------------------------------------
if (Test-Path $BuildDir) { Remove-Item -Recurse -Force $BuildDir }
if (Test-Path $StageDir) { Remove-Item -Recurse -Force $StageDir }

New-Item -ItemType Directory -Path $BuildDir | Out-Null
New-Item -ItemType Directory -Path $StageDir | Out-Null
New-Item -ItemType Directory -Path $DistDir  -Force | Out-Null

& cmake -S $NativeDir -B $BuildDir `
    -DCMAKE_BUILD_TYPE=$BuildType `
    -DCMAKE_INSTALL_PREFIX=$StageDir `
    -DMAGNAUNDASONI_BUILD_TESTS=OFF `
    -DMAGNAUNDASONI_ENABLE_SIMD=$EnableSimd `
    -DMAGNAUNDASONI_ENABLE_RT=$EnableRt
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed (exit $LASTEXITCODE)" }

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
& cmake --build $BuildDir --config $BuildType --parallel
if ($LASTEXITCODE -ne 0) { throw "CMake build failed (exit $LASTEXITCODE)" }

# ---------------------------------------------------------------------------
# Install to staging area
# ---------------------------------------------------------------------------
& cmake --install $BuildDir --config $BuildType
if ($LASTEXITCODE -ne 0) { throw "CMake install failed (exit $LASTEXITCODE)" }

# ---------------------------------------------------------------------------
# Add a version manifest
# ---------------------------------------------------------------------------
$BuiltAt = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
@"
version=$Version
platform=windows
arch=x64
build_type=$BuildType
built_at=$BuiltAt
"@ | Set-Content -Encoding UTF8 (Join-Path $StageDir "VERSION")

# ---------------------------------------------------------------------------
# Zip
# ---------------------------------------------------------------------------
$ArchiveOut = Join-Path $DistDir "$ArchiveName.zip"

# Use Compress-Archive (PowerShell 5+) with a wildcard so the top-level dir
# inside the zip is the staging folder name, not an absolute path.
Compress-Archive -Path "$StageDir\*" -DestinationPath $ArchiveOut -Force

Write-Host "==> Archive created: $ArchiveOut"

# Clean staging directory
Remove-Item -Recurse -Force $StageDir
