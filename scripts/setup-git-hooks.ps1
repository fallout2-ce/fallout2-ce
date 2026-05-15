# Point this repository at .githooks/ (pre-commit formats staged C++ files).

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $Root

if (-not (Test-Path ".githooks\pre-commit")) {
    throw "Missing .githooks\pre-commit"
}

git config --local core.hooksPath .githooks

Write-Host "Installed git hooks (core.hooksPath=.githooks)."
Write-Host "pre-commit runs via Git's shell (Git Bash on Windows) and formats staged *.cc / *.h when clang-format 14 is available."
