# RavAva Native Android Demo (API 21+)

Pure native (C++ / JNI) demo of [RavAva](https://github.com/crazyfull/RavAva) for **Android 5.0+**.  
No Qt.

## Features

- **Loopback** – microphone → Opus → jitter buffer → speaker
- **UDP Voice Chat** – same pipeline over UDP
- Foreground service (audio survives minimize)
- Level meters + jitter-buffer stats
- ABIs: `armeabi-v7a` + `arm64-v8a`
- WebRTC **OFF**

---

## 1. Place the folder

```
crazyfull/RavAva/
├── src/ravava/
├── miniaudio/
├── android4-demo/          ← extract the zip here (rename if needed)
│   ├── app/
│   ├── gradle/
│   ├── gradlew
│   ├── build.gradle
│   └── ...
└── .github/workflows/
```

The CMakeLists walks up 5 levels (`../../../../..`) to find the library sources.

---

## 2. GitHub Actions (recommended)

GitHub only runs workflows that live under the **repository root**:

```
crazyfull/RavAva/.github/workflows/build-native-android.yml
```

So after extracting:

```bash
# from the RavAva repo root
cp android4-demo/.github/workflows/build-native-android.yml \
   .github/workflows/build-native-android.yml
git add .github/workflows/build-native-android.yml android4-demo
git commit -m "Add native Android demo + CI"
git push
```

Then:

1. Go to the **Actions** tab
2. Select **Build Native Android Demo**
3. Click **Run workflow**
4. Choose ABI (`all` / `arm64-v8a` / `armeabi-v7a`)
5. Download the APK artifacts when the job finishes

---

## 3. Local build (optional)

```bash
cd android4-demo
./gradlew assembleDebug
# APK → app/build/outputs/apk/debug/app-debug.apk
```

Requirements: JDK 17, Android SDK + NDK 25+, CMake 3.22+.  
First build downloads Opus via FetchContent (needs internet).

---

## Permissions

- `RECORD_AUDIO` (runtime)
- `MODIFY_AUDIO_SETTINGS`
- `INTERNET` (UDP)
- `FOREGROUND_SERVICE` + `FOREGROUND_SERVICE_MICROPHONE`

Logs: `adb logcat -s ravava`

Use headphones for loopback.
