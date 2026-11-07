# RavAva Native Android Demo (API 21+)

Pure native (C++ / JNI) demo of [RavAva](https://github.com/crazyfull/RavAva) for Android 5.0 and newer.  
No Qt.

## Features

- **Loopback** – microphone → Opus → jitter buffer → speaker
- **UDP Voice Chat** – same pipeline over UDP (optional network envelope)
- Foreground service so audio continues when the app is minimized
- Level meters + basic jitter-buffer stats
- Builds `armeabi-v7a` + `arm64-v8a`
- No WebRTC (echo cancellation / NS / AGC disabled)

## Placement

This folder is meant to live **inside** the RavAva repository:

```
crazyfull/RavAva/
├── src/ravava/          ← library sources
├── miniaudio/
├── android4-demo/       ← this project (or rename)
│   ├── app/
│   ├── build.gradle
│   └── ...
└── ...
```

The CMakeLists.txt walks up (`../../../../..`) to find the library sources.  
If you put the demo somewhere else, edit the `RAVAVA_ROOT` path in `app/src/main/cpp/CMakeLists.txt`.

## Requirements

- Android Studio Hedgehog (2023.1.1) or newer (or command-line SDK + NDK)
- NDK 25+ (or the one bundled with Android Studio)
- CMake 3.22+
- Internet access on the first build (Opus is fetched via FetchContent)

## Build with Android Studio

1. Open the `android4-demo` (or `android5-native-demo`) folder as a project.
2. Let Gradle sync (it will download the Android Gradle Plugin and dependencies).
3. Connect a device / start an emulator (API 21+).
4. Run ▶

## Build from command line

```bash
# from the demo root
./gradlew assembleDebug

# APK location:
# app/build/outputs/apk/debug/app-debug.apk
```

## Permissions

- `RECORD_AUDIO` (runtime)
- `MODIFY_AUDIO_SETTINGS`
- `INTERNET` / `ACCESS_NETWORK_STATE` (for UDP)
- `FOREGROUND_SERVICE` + `FOREGROUND_SERVICE_MICROPHONE`
- Bluetooth permissions are declared for future headset support but not required for basic operation.

## Usage tips

- **Loopback**: use headphones, otherwise the speaker feeds back into the microphone.
- **UDP**: set the same local port on both devices (or different ports) and point each side at the other’s IP.
- Logs: `adb logcat -s ravava`

## GitHub Actions

A ready-to-use workflow is in `.github/workflows/build-native-android.yml`.  
It builds debug APKs for both ABIs and uploads them as artifacts.

## Safety Rules

This demo follows the rules listed in the main RavAva README (section 4):

- `AudioConfig` outlives Recorder / Player
- `SetPacketHandler` before `Start`
- No blocking inside the packet handler
- `KeepDevice()` called periodically
- `Stop()` then destroy
- `ravava::android::Init()` called once at startup

## License

The demo code itself can be used under the same terms as the rest of the RavAva repository (proprietary).  
Opus is under its own BSD license.
