# Contributing

This file is currently a shell to be filled in over time. For now, it only captures project-specific maintenance notes that are easy to miss.

For current sfall compatibility status and the remaining work needed to close gaps, see [SFALL_COMPATIBILITY.md](SFALL_COMPATIBILITY.md).

## Building

The project uses CMake, so building mostly comes down to picking the right preset.

## Commandline arguments

- `--scan-unimplemented` - do an analysis of opcodes and hooks used by loaded mods
- `--dev-load-game=1` - load the given save game automatically (useful for LLM automation)

### macOS

- Install the Command Line Tools if needed: `xcode-select --install`
- Install the common build dependencies: `brew install cmake ninja`
- Configure and build:

```sh
cmake -S . -B out/build/macos -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/macos --target fallout2-ce
```

### Linux

Install SDL and a toolchain:

- Debian / Ubuntu:
```sh
sudo apt install build-essential cmake ninja-build libsdl2-dev zlib1g-dev
```
- Fedora:
```sh
sudo dnf install gcc-c++ cmake ninja-build SDL2-devel zlib-devel
```
- Arch Linux:
```sh
sudo pacman -S base-devel cmake ninja sdl2 zlib
```

Then build:
```
cmake -S . -B out/build/linux-x64-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/linux-x64-debug --target fallout2-ce
```

### Windows

TODO: add Visual Studio build instructions. This will likely need a Visual Studio generator or preset plus the MSVC toolchain and Windows SDK.

## `.dat` CLI Tool

There is a small command-line archive tool in this repo for inspecting Fallout `.dat` files. It supports both Fallout 2 and Fallout 1 archives.

From the repo root, build it with your normal CMake workflow, targeting `ce-dat-tool`:

`cmake --build out/build/<preset-name> --target ce-dat-tool`

The executable is written to your selected build directory, so the exact path will vary by preset and configuration.

The current tool is read-only. Available commands:

1. `./<BUILD_DIR>/ce-dat-tool <archive.dat> list [pattern]`
2. `./<BUILD_DIR>/ce-dat-tool <archive.dat> info [pattern]`
3. `./<BUILD_DIR>/ce-dat-tool <archive.dat> extract [--lower] <output-dir> [pattern]`
4. `./<BUILD_DIR>/ce-dat-tool <archive.dat> cat <entry>`

Use `--lower` with `extract` when you want every extracted file and directory name forced to lowercase.
For example:

`ce-dat-tool master.dat extract --lower /tmp/ce-dat-tool-lower data\\*`

## Updating SDL

SDL is pinned for native builds in `third_party/sdl2/CMakeLists.txt`. Right now, Android also relies on checked-in Java bindings in `os/android/app/src/main/java/org/libsdl/app`, and those bindings must match the SDL version fetched by CMake.

When updating SDL:

1. Update the SDL tag in `third_party/sdl2/CMakeLists.txt`.
2. Run an Android build once so Gradle/CMake fetch the new SDL source tree.
3. Copy the Android Java bindings from the fetched SDL checkout into `os/android/app/src/main/java/org/libsdl/app`.
   Source path pattern:
   `os/android/app/.cxx/<Variant>/<Hash>/arm64-v8a/_deps/sdl2-src/android-project/app/src/main/java/org/libsdl/app`
4. Rebuild Android and confirm the SDL sync guard passes.

The Android build contains a guard in `os/android/app/build.gradle` that fails if the checked-in Java bindings drift from the fetched native SDL source.

This is a hack, not a good long-term setup. The Java bindings are still duplicated in the repo instead of being sourced from the same SDL tree as the native build. That should be improved so Android does not depend on a manual copy/sync step.
