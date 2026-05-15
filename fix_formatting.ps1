# Format src/ with clang-format 14 (matches CI). For Windows (PowerShell / Visual Studio devs).
# Requires clang-format 14 on PATH (LLVM install or VS Clang tools).

$ErrorActionPreference = "Stop"

$RequiredMajor = 14

function Get-ClangFormat14 {
    foreach ($name in @("clang-format-14", "clang-format")) {
        $cmd = Get-Command $name -ErrorAction SilentlyContinue
        if (-not $cmd) { continue }
        $versionLine = & $cmd.Source --version 2>&1 | Select-Object -First 1
        if ($versionLine -match "version\s+(\d+)") {
            if ([int]$Matches[1] -eq $RequiredMajor) {
                return $cmd.Source
            }
        }
    }
    return $null
}

function Get-SourceFiles {
    Get-ChildItem -Path "src" -Recurse -Include "*.cc", "*.h" | ForEach-Object { $_.FullName }
}

function Show-InstallHelp {
    Write-Host @"
clang-format 14 is required but was not found on PATH.

Windows:
  winget install -e --id LLVM.LLVM
  Or install LLVM 14.x from https://github.com/llvm/llvm-project/releases/tag/llvmorg-14.0.6
  Add LLVM\bin to PATH, then reopen the terminal.

Visual Studio:
  VS Installer -> modify -> "C++ Clang tools for Windows"
  Tools -> Options -> Text Editor -> C/C++ -> Formatting ->
    Use custom clang-format executable -> path to clang-format.exe (14.x)

Linux / macOS:
  Use ./fix_formatting.sh instead (native clang-format 14 or Docker fallback).

"@ -ForegroundColor Yellow
}

$clangFormat = Get-ClangFormat14
if (-not $clangFormat) {
    Show-InstallHelp
    exit 1
}

$files = Get-SourceFiles
if ($files.Count -eq 0) {
    Write-Error "No source files under src/"
}

$mode = if ($args.Count -gt 0) { $args[0] } else { "--fix" }

switch ($mode) {
    "--version" {
        & $clangFormat --version
    }
    { $_ -in "--fix", "" } {
        & $clangFormat -i @files
        Write-Host "Formatted $($files.Count) files with $clangFormat"
    }
    "--check" {
        & $clangFormat --dry-run --Werror @files
        Write-Host "Format check passed."
    }
    "--dry-run" {
        & $clangFormat --dry-run @files
    }
    default {
        Write-Host "Usage: .\fix_formatting.ps1 [--fix|--check|--dry-run|--version]"
        exit 1
    }
}
