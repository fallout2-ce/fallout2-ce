# Format src/ with clang-format 14 (matches CI). For Windows (PowerShell / Visual Studio).
# Uses local clang-format 14 when on PATH; falls back to Docker if available.

$ErrorActionPreference = "Stop"

$RequiredMajor = 14
$ClangImage = "silkeh/clang:14"

function Show-Usage {
    Write-Host "Usage: .\fix_formatting.ps1 [--fix|--check|--dry-run|--version]"
}

function Show-InstallHelp {
    Write-Host @"
clang-format 14 is required but was not found on PATH, and Docker is not available.

Windows:
  Install LLVM 14.0.6 (recommended):
  https://github.com/llvm/llvm-project/releases/tag/llvmorg-14.0.6
  Add LLVM\bin to PATH and reopen the terminal.

  Or install Docker Desktop and re-run this script (uses $ClangImage).

Visual Studio:
  VS Installer -> modify -> "C++ Clang tools for Windows"
  Tools -> Options -> Text Editor -> C/C++ -> Formatting ->
    Use custom clang-format executable -> LLVM 14 clang-format.exe

Linux / macOS:
  ./fix_formatting.sh

"@ -ForegroundColor Yellow
}

function Test-Docker {
    if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
        return $false
    }
    docker info 2>&1 | Out-Null
    return $LASTEXITCODE -eq 0
}

function Get-ClangFormat14 {
    foreach ($name in @("clang-format-14", "clang-format")) {
        $cmd = Get-Command $name -ErrorAction SilentlyContinue
        if (-not $cmd) { continue }
        $versionLine = & $cmd.Source --version 2>&1 | Select-Object -First 1
        if ($versionLine -match "version\s+(\d+)") {
            $major = [int]$Matches[1]
            if ($major -eq $RequiredMajor) {
                return $cmd.Source
            }
            Write-Warning "$name is version $major (need $RequiredMajor); ignoring."
        }
    }
    return $null
}

function Assert-RepoRoot {
    if (-not (Test-Path "src")) {
        throw "Run from the repository root (missing src/)."
    }
    if (-not (Test-Path ".clang-format")) {
        throw "Run from the repository root (missing .clang-format)."
    }
}

function Invoke-DockerClangFormat {
    param(
        [ValidateSet("fix", "check", "dry-run")]
        [string]$Mode
    )

    $root = (Get-Location).Path
    Write-Host "note: using Docker ($ClangImage). Install clang-format 14 locally to skip Docker."

    $bashCmd = switch ($Mode) {
        "fix"     { 'find src -type f \( -name "*.cc" -o -name "*.h" \) -print0 | xargs -0 -r clang-format -i' }
        "check"   { 'find src -type f \( -name "*.cc" -o -name "*.h" \) -print0 | xargs -0 -r clang-format --dry-run --Werror' }
        "dry-run" { 'find src -type f \( -name "*.cc" -o -name "*.h" \) -print0 | xargs -0 -r clang-format --dry-run' }
    }

    docker run --rm `
        -v "${root}:/app" `
        -w /app `
        $ClangImage `
        bash -c $bashCmd

    if ($LASTEXITCODE -ne 0) {
        throw "docker clang-format failed (exit $LASTEXITCODE)"
    }
}

function Get-SourceFiles {
    $root = (Get-Location).Path
    Get-ChildItem -Path "src" -Recurse -File -Include "*.cc", "*.h" |
        ForEach-Object { $_.FullName.Substring($root.Length + 1) -replace '\\', '/' }
}

function Invoke-NativeClangFormat {
    param(
        [string[]]$ClangArgs,
        [string[]]$Files
    )

    & $clangFormat @ClangArgs @Files
    if ($LASTEXITCODE -ne 0) {
        throw "clang-format failed with exit code $LASTEXITCODE"
    }
}

Assert-RepoRoot

$clangFormat = Get-ClangFormat14
$useDocker = (-not $clangFormat) -and (Test-Docker)

if (-not $clangFormat -and -not $useDocker) {
    Show-InstallHelp
    exit 1
}

$mode = if ($args.Count -gt 0) { $args[0] } else { "--fix" }

switch ($mode) {
    "--version" {
        if ($clangFormat) {
            & $clangFormat --version
        }
        else {
            docker run --rm $ClangImage clang-format --version
        }
    }
    "--fix" {
        if ($useDocker) {
            Invoke-DockerClangFormat "fix"
            Write-Host "Formatted src/ via Docker."
        }
        else {
            $files = @(Get-SourceFiles)
            if ($files.Count -eq 0) { throw "No .cc/.h files under src/" }
            Invoke-NativeClangFormat -ClangArgs @("-i") -Files $files
            Write-Host "Formatted $($files.Count) files with $clangFormat"
        }
    }
    "--check" {
        if ($useDocker) {
            Invoke-DockerClangFormat "check"
        }
        else {
            $files = @(Get-SourceFiles)
            Invoke-NativeClangFormat -ClangArgs @("--dry-run", "--Werror") -Files $files
        }
        Write-Host "Format check passed."
    }
    "--dry-run" {
        if ($useDocker) {
            Invoke-DockerClangFormat "dry-run"
        }
        else {
            $files = @(Get-SourceFiles)
            Invoke-NativeClangFormat -ClangArgs @("--dry-run") -Files $files
        }
    }
    default {
        Show-Usage
        exit 1
    }
}
