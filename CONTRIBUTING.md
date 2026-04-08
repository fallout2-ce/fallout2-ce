# Contributing

This file is currently a shell to be filled in over time. For now, it only captures project-specific maintenance notes that are easy to miss.

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
