# RavAva on Android

The Android library is built by the **Build static library Android** workflow
(`.github/workflows/build-static-library-android.yml`: GitHub -> Actions -> *Build static library Android* -> Run workflow ->
artifact **`RavAva-Android-static`**, a zip with the folders below and this file).

    RavAva-Android-static/
      include/                         every header an app needs (ravava/*.h, opus.h & co)
      lib/arm64-v8a/libRavAva.a        64-bit ARM phones (almost all today)            with webrtc
      lib/x86_64/libRavAva.a           emulator / Chromebook (x86_64)                  with webrtc
      lib/x86/libRavAva.a              old 32-bit emulator                             WITHOUT webrtc
      lib/armeabi-v7a/libRavAva.a      old 32-bit ARM phones (Android 5, Galaxy S4)    WITHOUT webrtc
      lib/<abi>/BUILD-INFO.txt         what is inside that file (webrtc yes/no, minSdk, NDK, STL)

Every `libRavAva.a` is **one** static library with everything inside: RavAva + Opus (+ webrtc-audio-processing: echo
cancellation, noise suppression, AGC, low-cut). Nothing else has to be built or installed.

**Why armeabi-v7a and x86 have no webrtc:** webrtc needs API 24 on the 32-bit ABIs, and RavAva supports Android 5 (API 21). So
the 32-bit files have no echo canceller (`AudioProcessor::IsAvailable()` is false there; use a headset on those phones).
arm64-v8a and x86_64 have it and still work from API 21.

## What you need (dependencies)

| Needed | Why / version |
|---|---|
| Android 5.0 (API 21) or newer | `minSdk 21`; AAudio is used from API 26, OpenSL ES below |
| NDK r26 (26.1.10909125 was used) | the library was built with it; a newer NDK is fine for the app |
| **STL `c++_shared`** | the `.a` does not contain the C++ runtime: `-DANDROID_STL=c++_shared` (Gradle: `arguments "-DANDROID_STL=c++_shared"`). Package `libc++_shared.so` (Gradle does it) |
| C++17 | `-std=c++17` (and `-fexceptions -frtti`) |
| System libraries (link them) | `log`, `android`, `OpenSLES`, `dl`, `m` |
| Permissions | `RECORD_AUDIO`, `MODIFY_AUDIO_SETTINGS`; `BLUETOOTH_CONNECT` (Android 12+) for Bluetooth headsets; for a call in the background a foreground service of type `microphone` (`FOREGROUND_SERVICE`, `FOREGROUND_SERVICE_MICROPHONE`) |
| Runtime permission | ask for `RECORD_AUDIO` (and `BLUETOOTH_CONNECT`) before `Recorder::Start()` |
| JNI call once | `ravava::android::Init(JavaVM*, jobject applicationContext)` (see the demo's `native-lib.cpp`) |

You do **not** need to install Opus, webrtc, meson or anything else. Everything is in the `.a`.

## Add it to an app (Gradle + CMake)

1. Unzip `RavAva-Android-static.zip` somewhere, for example `app/src/main/cpp/ravava/`.
2. `app/build.gradle`:

   ```groovy
   android {
       defaultConfig {
           minSdk 21
           ndk { abiFilters 'arm64-v8a', 'armeabi-v7a' }          // the ABIs you ship
           externalNativeBuild { cmake {
               cppFlags "-std=c++17 -fexceptions -frtti"
               arguments "-DANDROID_STL=c++_shared"
           } }
       }
       externalNativeBuild { cmake { path "src/main/cpp/CMakeLists.txt" } }
   }
   ```

3. `src/main/cpp/CMakeLists.txt`:

   ```cmake
   set(RAVAVA ${CMAKE_CURRENT_SOURCE_DIR}/ravava)          # the unzipped folder
   add_library(ravava_static STATIC IMPORTED)
   set_target_properties(ravava_static PROPERTIES
       IMPORTED_LOCATION ${RAVAVA}/lib/${ANDROID_ABI}/libRavAva.a
       INTERFACE_INCLUDE_DIRECTORIES "${RAVAVA}/include;${RAVAVA}/include/ravava"
       INTERFACE_LINK_LIBRARIES "log;android;OpenSLES;dl;m")

   add_library(myvoice SHARED native-lib.cpp)               # your JNI code
   target_link_libraries(myvoice ravava_static)
   ```

4. In your JNI code (C++): `#include <ravava/Recorder.h>`, `#include <ravava/Player.h>`; the API, the safety rules and the recipes
   are in the main `README.md` (sections 6, 7, 9, 11). Call `ravava::android::Init(vm, context)` once, and forward audio-focus
   changes with `ravava::android::SetAudioFocusLost(bool)`.
5. The complete working example is `build-demo/android-native/` (Java UI + JNI): it builds with `-PravavaPrebuilt=<unzipped folder>`
   exactly like this (see its `CMakeLists.txt`).

## If the build fails
- `undefined reference to ... std::` / `__cxa_...`: the app uses another STL: set `-DANDROID_STL=c++_shared`.
- `undefined reference to AAudio... / SL_IID_...`: link `OpenSLES` and `android`, and `dl`.
- `cannot locate symbol ... fseeko` on API 21..23: you used a webrtc build for a 32-bit ABI; use the `armeabi-v7a` / `x86` file from this package (without webrtc).
- `dlopen failed: library "libc++_shared.so" not found`: the STL is not packaged (Gradle packages it when `ANDROID_STL=c++_shared`).
- Silence / no microphone: `RECORD_AUDIO` not granted before `Recorder::Start()`.
- `Recorder` reports `MicEvent::Busy`: a call or another app has the microphone (see the README, "Microphone events").

## How it is built
One job per ABI: Opus 1.5.2 and (not on the 32-bit ABIs) webrtc-audio-processing 1.3 (`libs/webrtc/build_android.sh`, cached) are built
with the NDK, RavAva is built with CMake (`src/ravava`), the three archives are merged with `llvm-ar`, a small program is linked
against the result, and a last job collects every ABI into `RavAva-Android-static`. If the webrtc step of an ABI fails, that ABI
is built without echo cancellation and the workflow shows a warning (`BUILD-INFO.txt` says which).

## Licenses

`licenses/` next to `lib/` and `include/` holds the license texts of everything inside `libRavAva.a` (Opus, webrtc-audio-processing,
abseil, …) and `THIRD_PARTY_NOTICES.md`. An app that ships this library must show / include them (BSD-3-Clause and Apache-2.0
require the text to travel with the binary), e.g. in an in-app "Open source licenses" screen.
