# Contributing

This file is currently a shell to be filled in over time. For now, it only captures project-specific maintenance notes that are easy to miss.

## `.dat` CLI Tool

There is a small command-line archive tool in this repo for inspecting Fallout `.dat` files. It supports both Fallout 2 and Fallout 1 archives.

Build it with `make TARGET=fallout2-dat`.

The executable is written to your selected `BUILD_DIR`, so the default path is `out/build/local-debug-arm64/fallout2-dat` on this macOS setup, but other platforms or custom build directories will differ.

The current tool is read-only. Available commands:

1. `./<BUILD_DIR>/fallout2-dat <archive.dat> list [pattern]`
2. `./<BUILD_DIR>/fallout2-dat <archive.dat> info [pattern]`
3. `./<BUILD_DIR>/fallout2-dat <archive.dat> extract [--lower] <output-dir> [pattern]`
4. `./<BUILD_DIR>/fallout2-dat <archive.dat> cat <entry>`

Use `--lower` with `extract` when you want every extracted file and directory name forced to lowercase.
For example, from `/Applications/Fallout2Codex` you can run:

`/Users/klaas/game/fallout2-ce/out/build/local-debug-arm64/fallout2-dat master.dat extract --lower /tmp/fallout2-dat-lower data\\*`

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
