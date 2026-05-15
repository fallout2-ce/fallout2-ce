# Format src/ with clang-format 14 (matches CI). For Windows (PowerShell / Visual Studio).
# Requires clang-format 14 on PATH — see Show-InstallHelp.

$ErrorActionPreference = "Stop"

$RequiredMajor = 14

function Show-Usage {
    Write-Host "Usage: .\fix_formatting.ps1 [--fix|--check|--dry-run|--version]"
}

function Show-InstallHelp {
    Write-Host @"
clang-format 14 is required but was not found on PATH.

Windows:
  winget install -e --id LLVM.LLVM
  Or LLVM 14.0.6: https://github.com/llvm/llvm-project/releases/tag/llvmorg-14.0.6
  Add LLVM\bin to PATH and reopen the terminal.

Visual Studio:
  VS Installer -> modify -> "C++ Clang tools for Windows"
  Tools -> Options -> Text Editor -> C/C++ -> Formatting ->
    Use custom clang-format executable -> LLVM 14 clang-format.exe

Linux / macOS:
  ./fix_formatting.sh

"@ -ForegroundColor Yellow
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

function Get-SourceFiles {
    if (-not (Test-Path "src")) {
        throw "Run from the repository root (missing src/)."
    }
    if (-not (Test-Path ".clang-format")) {
        throw "Run from the repository root (missing .clang-format)."
    }
    @(
        Get-ChildItem -Path "src" -Recurse -File -Include "*.cc", "*.h" |
            ForEach-Object { $_.FullName }
    )
}

$clangFormat = Get-ClangFormat14
if (-not $clangFormat) {
    Show-InstallHelp
    exit 1
}

$files = Get-SourceFiles
if ($files.Count -eq 0) {
    throw "No .cc/.h files under src/"
}

$mode = if ($args.Count -gt 0) { $args[0] } else { "--fix" }

switch ($mode) {
    "--version" {
        & $clangFormat --version
    }
    "--fix" {
        foreach ($file in $files) {
            & $clangFormat -i $file
        }
        Write-Host "Formatted $($files.Count) files with $clangFormat"
    }
    "--check" {
        foreach ($file in $files) {
            & $clangFormat --dry-run --Werror $file
        }
        Write-Host "Format check passed ($($files.Count) files)."
    }
    "--dry-run" {
        foreach ($file in $files) {
            & $clangFormat --dry-run $file
        }
    }
    default {
        Show-Usage
        exit 1
    }
}
