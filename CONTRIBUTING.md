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

Available commands:

1. `./<BUILD_DIR>/ce-dat-tool <archive.dat> list [pattern]`
2. `ce-dat-tool <archive.dat> info [pattern]`
3. `ce-dat-tool <archive.dat> extract [--lower] <output-dir> [pattern]`
4. `ce-dat-tool <archive.dat> extract [--lower] (--file-list|--files-from) <list-file> <output-dir>`
5. `ce-dat-tool <archive.dat> cat <entry>`
6. `ce-dat-tool create <input-dir> <archive.dat>`

Use `--lower` with `extract` when you want every extracted file and directory name forced to lowercase.
For example:

`ce-dat-tool master.dat extract --lower /tmp/ce-dat-tool-lower data\\*`

## `ce-frm2png` CLI Tool

There is also a small FRM conversion tool in this repo for converting Fallout art between `.frm` and `.png`.

From the repo root, build it with your normal CMake workflow, targeting `ce-frm2png`:

`cmake --build out/build/<preset-name> --target ce-frm2png`

The executable is written to your selected build directory, so the exact path will vary by preset and configuration.

Basic usage:

1. `./<BUILD_DIR>/ce-frm2png <input.frm> [output.png]`
2. `./<BUILD_DIR>/ce-frm2png <input.frm> [output.png] --palette <path-to-color.pal>`
3. `./<BUILD_DIR>/ce-frm2png <input.frm> [output.png] --frame <index> --direction <index>`
4. `./<BUILD_DIR>/ce-frm2png <input.png> [output.frm] --palette <path-to-color.pal>`
5. `./<BUILD_DIR>/ce-frm2png <input.png> [output.frm] --fps 10 --action-frame 0 --x-offset 0 --y-offset 0`
6. `./<BUILD_DIR>/ce-frm2png <input.png> <output.png> --palette <path-to-color.pal>`
7. `./<BUILD_DIR>/ce-frm2png - [output.png|-] --from-frm --palette <path-to-color.pal>`
8. `./<BUILD_DIR>/ce-frm2png - [output.frm|-] --from-png --palette <path-to-color.pal>`

`-` can be used to take input from stdin (e.g. from `ce-dat-tool cat`) or write to stdout. When reading from stdin, pass `--from-frm` or `--from-png`. Output defaults to PNG for FRM input and FRM for PNG input when output is omitted or `-`; use `--to-png` for PNG output to stdout from PNG input.

Output file extensions are validated against the selected conversion. For example, PNG-to-FRM output must use `.frm`, and PNG output must use `.png`.

If `--palette` isn't specified, it will pick up `color.pal` in the input file's directory or cwd.

Notes:

- The tool expects a raw `color.pal` file. It does not read palettes directly from `master.dat`, so extract `color.pal` first if needed.
- A typical extraction flow is: `ce-dat-tool master.dat extract --lower . 'color.pal'`
- By default palette index `0` becomes transparent in the output PNG. Pass `--opaque` to keep it opaque.
- PNG output is always palette-indexed and preserves Fallout palette indices when possible.
- For `png -> frm`, transparent pixels (alpha below 128) map to palette index `0` by default. Pass `--opaque` to quantize every pixel instead.
- The current `png -> frm` implementation writes a single-frame FRM and points all six direction slots at the same frame payload.

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
