# Geobot

A macro recorder, player, and clickbot for Geometry Dash **2.2091**.

## Building

Use the [Geode 6.0.0-beta.2 SDK](https://github.com/geode-sdk/geode/releases/tag/v6.0.0-beta.2),
Geode CLI 4.0.0 or newer, and the current `geode-sdk/bindings` main branch.
Set `GEODE_SDK` to the SDK checkout and install binaries for the same SDK version
and your target platform. Run the mod with Geode 6.0.0-beta.2 or a compatible
newer loader. Earlier alpha loaders are not compatible with this build.

GitHub Actions builds Windows, Android32, and Android64 using the SDK version
specified in `mod.json`, including matching release binaries.
iOS support is pending the missing 2.2091 hook addresses in the official bindings.
For a local Android64 build, install Android NDK r29 and run:

```sh
cmake -S . -B build-android64 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-23 \
  -DANDROID_STL=c++_shared \
  -DGEODE_TARGET_PLATFORM=Android64 \
  -DGEODE_DONT_INSTALL_MODS=ON
cmake --build build-android64 --parallel 4
```

Configuration downloads the mod dependencies from the Geode index. The resulting
`.geode` package requires Geometry Dash 2.2091 and Geode 6.0.0-beta.2 or a compatible newer loader.
