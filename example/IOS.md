# RavAva on iOS

Built by the **Build iOS** workflow (`.github/workflows/build-ios.yml`): `RavAva.xcframework` (device arm64 + simulator arm64,
Opus inside, no echo cancellation). It proves the library compiles and links; the audio itself has to be tried on an iPhone.

## Use it in an app
1. Drag `RavAva.xcframework` into the Xcode target ("Frameworks, Libraries"). Headers are inside (`#include <ravava/Recorder.h>`).
2. Link `AVFoundation`, `AudioToolbox`, `Foundation`, `CoreFoundation` (Xcode adds them when asked for the symbols).
3. `Info.plist`: `NSMicrophoneUsageDescription` (the app is killed on the first microphone use without it).
4. Ask for the microphone permission (`AVAudioApplication.requestRecordPermission`) BEFORE `Recorder::Start()`.
5. The audio session is set by the library: play-and-record, speaker (not the earpiece), Bluetooth headsets allowed.
   Call it from Swift through an Objective-C++ (`.mm`) file: RavAva is C++.
6. Optional: "Audio, AirPlay, and Picture in Picture" background mode so a call survives the screen lock.

## Echo cancellation (webrtc)
`libs/webrtc/build_ios.sh` builds webrtc-audio-processing 1.3 for iOS (arm64); the **Build Demo iOS** workflow does it (cached) and
links it into libRavAva (`-DRAVAVA_WITH_WEBRTC=ON -DRAVAVA_WEBRTC_ROOT=...`). The xcframework workflow (`build-ios.yml`) is still without it.

## Interruptions
A call / Siri / another app taking the audio session is reported as `MicEvent::Interrupted` (`MicReason::SystemInterruption`) and
`MicEvent::Free` afterwards; the speaker is re-opened by `Player::KeepDevice()` and the microphone by `automaticResumeAfterFreeMic`.
