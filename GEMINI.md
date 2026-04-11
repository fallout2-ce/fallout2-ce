# Fallout 2 Community Edition - Gemini Instructions

## Project Overview
Fallout 2 Community Edition is a re-implementation of Fallout 2. It is written in C++ and uses CMake as its build system.

## Project Rules
- Only search for source code and symbols inside the `src/` directory.

## Coding Standards & Conventions
- **Language**: C++17.
- **Style**: Follow WebKit style as defined in `.clang-format`.
  - Use `clang-format` before committing any changes.
  - Short `if` statements without `else` can be on a single line.
  - Fix namespace comments.
- **File Naming**:
  - Source files: `.cc`
  - Header files: `.h`
- **Architectural Constraint**:
  - Don't use complicated and rarely used C++ features, unless they are well encapsulated within reusable code.
  - Avoid large-scale refactorings. The project aims to reconcile changes from the Reference Edition, and major structural changes make this difficult.
- **Integration Priority**: Integrating features and fixes from Sfall is a top priority.

## Build System (CMake)
- **Version**: Requires CMake 3.18 or higher.
- **Dependencies**: Uses vendored third-party libraries by default (`FALLOUT_VENDORED=ON`).
- **Target**: The main executable is `fallout2-ce`.
- **Platforms**: Supports Windows, Linux, macOS, Android, iOS, and Emscripten (WebAssembly).

## Project Structure
- `src/`: Contains all core game logic and engine implementation.
- `third_party/`: Vendored dependencies (zlib, SDL2, etc.).
- `os/`: Platform-specific assets and configuration (icons, plists, etc.).
- `cmake/`: Custom CMake scripts.
- `files/`: Default configuration files and data assets.
- `sfall_testing/`: SSL scripts for testing Sfall-related functionality.

## Tool Usage
- **Prioritize CLion MCP**: Always use the CLion MCP tools (e.g., `mcp_clion_search_symbols`, `mcp_clion_get_symbol_info`) for finding code definitions and symbols instead of performing manual text-based searches with `grep_search` or `glob`. Use manual text searches only when symbol-based search is not applicable (e.g., searching for non-code text or specific patterns).

## Development Workflow
1. **Research**: Understand how a feature was implemented in the original engine or Sfall before attempting to port or fix it.
2. **Strategy**: Maintain compatibility with original game data and savegames.
3. **Execution**: Apply surgical changes to `src/`.
4. **Validation**: Ensure changes compile across platforms if possible, or at least don't break the build for the current platform.
