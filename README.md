# Geobot

A macro recorder, player, and clickbot for Geometry Dash **2.2091**.

## Building

Use the [Geode nightly SDK](https://github.com/geode-sdk/geode/releases/tag/nightly)
(currently version **5.11.2**), Geode CLI 4.0.0 or newer, and the current
`geode-sdk/bindings` main branch. Set `GEODE_SDK` to the SDK checkout and install
its nightly binaries for your target platform. Stable SDKs targeting 2.2081 are
not compatible with this branch.

GitHub Actions builds Windows, Android32, and Android64 using the nightly SDK.
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
`.geode` package requires Geometry Dash 2.2091 and the matching Geode nightly.
