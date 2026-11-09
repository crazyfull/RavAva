# RavAva on iOS

Built by the **Build static library iOS** workflow (`.github/workflows/build-static-library-ios.yml`, Actions -> Run workflow ->
artifact `RavAva-iOS-static`). Device arm64, iOS 13+, **one file** with everything inside:

    RavAva-iOS/lib/libRavAva.a     RavAva + Opus + webrtc-audio-processing (echo cancellation, noise suppression, AGC) merged
    RavAva-iOS/include/            every header an app needs: ravava/*.h and opus.h & co

It proves the library compiles and links (a small device-SDK link test runs in CI); the audio itself has to be tried on an iPhone.
If the webrtc step fails the library is still built, without echo cancellation (the workflow shows a warning).
The same file is used by the iPhone demo (`build-demo-ios.yml` calls this workflow and only compiles the app).

## Use it in an app
1. Add `lib/libRavAva.a` to the Xcode target ("Frameworks, Libraries") and `include/` and `include/ravava/` to "Header Search Paths"
   (`#include <ravava/Recorder.h>`). Add `-ObjC` to "Other Linker Flags".
2. Link `AVFoundation`, `AudioToolbox`, `CoreAudio`, `CoreFoundation`, `CoreGraphics`, `Foundation`.
3. `Info.plist`: `NSMicrophoneUsageDescription` (the app is killed on the first microphone use without it).
4. Ask for the microphone permission (`AVAudioSession requestRecordPermission`) BEFORE `Recorder::Start()`.
5. The audio session is set by the library: play-and-record, speaker (not the earpiece), Bluetooth headsets allowed.
   Call it from Swift through an Objective-C++ (`.mm`) file: RavAva is C++.
6. Optional: "Audio, AirPlay, and Picture in Picture" background mode so a call survives the screen lock.

The iPhone Simulator needs its own build (not part of this workflow: device and simulator arm64 cannot share one `.a`).

## Echo cancellation (webrtc)
`libs/webrtc/build_ios.sh` builds webrtc-audio-processing 1.3 for iOS (arm64, cached by the workflow) and the workflow links it into
libRavAva (`-DRAVAVA_WITH_WEBRTC=ON -DRAVAVA_WEBRTC_ROOT=...`).

## Interruptions
A call / Siri / another app taking the audio session is reported as `MicEvent::Interrupted` (`MicReason::SystemInterruption`) and
`MicEvent::Free` afterwards; the speaker is re-opened by `Player::KeepDevice()` and the microphone by `automaticResumeAfterFreeMic`.
