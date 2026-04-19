## Further Reading
- Refer to the project README for overall architecture.

## Working With Code
- Only search for code and symbols inside the `src/` directory.
- NEVER search entire code base for certain file or symbol when it's location isn't known, ask first.
- NEVER read entire files unless absolutely necessary for task. Use MCP search instead.
- Prefer CLion MCP for searching code and basic refactoring over raw text tools such as grep.
- Test changes by compiling (separate cc file for small local changes, or via CLion build_project MCP for big changes).
- If game run test is request by prompt, start the game from C:\Games\Fallout2\@CE folder. It always contains last built executable, config file and game assets for full testing.

## Language Standard
- The project targets C++17 (`CMAKE_CXX_STANDARD 17` in `CMakeLists.txt`).
- Prefer C++17 library APIs and semantics when they simplify code. For example, `std::string::data()` is mutable in C++17.
- static global variable need no prefix.  exported globals get g*.
- generally prefer camelCase

## Project Structure
- `src/`: All core game logic and engine implementation.
- `third_party/`: Vendored dependencies (zlib, SDL2, etc.).
- `os/`: Platform-specific assets and configuration (icons, plists, etc.).
- `cmake/`: Custom CMake scripts.
- `files/`: Default configuration files and data assets.
- `sfall_testing/`: SSL scripts for testing Sfall-related functionality.

