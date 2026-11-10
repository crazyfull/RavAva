# RavAva on iOS

The iOS library is `RavAva.xcframework`, built by the **Build static library iOS** workflow
(`.github/workflows/build-static-library-ios.yml`: GitHub -> Actions -> *Build static library iOS* -> Run workflow ->
artifact **`RavAva-iOS-static`**, a zip with the folder below and this file). No Mac is needed to build it.

    RavAva.xcframework
      ios-arm64/                      iPhone / iPad (device)            libRavAva.a + Headers/
      ios-arm64-simulator/            iPhone Simulator (Apple-silicon Mac)   libRavAva.a + Headers/

Every slice is **one** static library with everything inside: RavAva + Opus + webrtc-audio-processing (echo cancellation,
noise suppression, AGC). Nothing else has to be built or installed. Xcode picks the right slice by itself and adds the header
folder for you.

## What you need (dependencies)

| Needed | Why / version |
|---|---|
| Xcode 15 or newer (iOS SDK) | to build your app (the library was built with the Xcode of GitHub's `macos-latest`) |
| iOS 13.0 or newer on the device | minimum deployment target of the library; your app may target a higher one |
| arm64 | iPhone 5s and newer (device); Apple-silicon Macs for the Simulator. **Intel-Mac simulator (x86_64) is not included** |
| System frameworks (link them) | `AVFoundation`, `AudioToolbox`, `CoreAudio`, `CoreFoundation`, `CoreGraphics`, `Foundation` |
| Linker flag | `-ObjC` (miniaudio's iOS part is Objective-C) |
| C++17 | the library is C++17 (libc++); the app target must use `CLANG_CXX_LANGUAGE_STANDARD = c++17` (or newer) |
| `Info.plist` | `NSMicrophoneUsageDescription` (required, the app is killed on the first microphone use without it) |
| `Info.plist` (optional) | `UIBackgroundModes` = `audio`, so a call survives the screen lock |
| `Info.plist` (if you talk to a server on the local network) | `NSLocalNetworkUsageDescription` |

You do **not** need to install Opus, webrtc, meson, CMake, or anything else. Everything is in the `.a`.

## Add it to an app (step by step)

1. Unzip `RavAva-iOS-static.zip`. Drag `RavAva.xcframework` into the Xcode project navigator
   (target membership: your app). Under *Target -> General -> Frameworks, Libraries, and Embedded Content* it must appear with
   **Do Not Embed** (it is a static library).
2. *Build Settings*: `Other Linker Flags` += `-ObjC`; `C++ Language Dialect` = `C++17`.
3. *Build Phases -> Link Binary With Libraries*: add `AVFoundation`, `AudioToolbox`, `CoreAudio`, `CoreFoundation`, `CoreGraphics`, `Foundation`.
4. *Info.plist*: add `NSMicrophoneUsageDescription` (a sentence for the user: "the app sends your voice to ...").
5. Write the code that uses RavAva in an **Objective-C++ file (`.mm`)**; RavAva is C++:

   ```objc
   // VoiceEngine.mm
   #import <AVFoundation/AVFoundation.h>
   #include <ravava/Recorder.h>
   #include <ravava/Player.h>
   using namespace ravava;
   ```

   From Swift, call a small Objective-C class you write in that `.mm` file (add its header to the bridging header). The demo
   (`build-demo/ios/Sources/main.mm`) is a complete working example: audio engine, UDP, settings, devices, music, microphone events.
6. Ask for the microphone permission **before** `Recorder::Start()`:

   ```objc
   [AVAudioSession.sharedInstance requestRecordPermission:^(BOOL granted) { if (granted) { /* start the recorder */ } }];
   ```

7. The audio session is set by the library (play-and-record, speaker instead of the earpiece, Bluetooth headsets allowed):
   do not reconfigure it from the app while a call is running.

### Swift Package Manager (optional)
Put the xcframework in a local package:

```swift
// Package.swift
let package = Package(
    name: "RavAvaBinary",
    products: [.library(name: "RavAva", targets: ["RavAva"])],
    targets: [.binaryTarget(name: "RavAva", path: "RavAva.xcframework")]
)
```
Add the package folder to the app ("Add Local..."), and still add `-ObjC` and the frameworks above.

### CocoaPods (optional)
```ruby
Pod::Spec.new do |s|
  s.name = 'RavAva'; s.version = '0.1.0'; s.summary = 'RavAva voice library'
  s.homepage = 'https://example.com'; s.license = { :type => 'Proprietary' }; s.author = 'RavAva'
  s.platform = :ios, '13.0'; s.source = { :path => '.' }
  s.vendored_frameworks = 'RavAva.xcframework'
  s.frameworks = 'AVFoundation', 'AudioToolbox', 'CoreAudio', 'CoreFoundation', 'CoreGraphics', 'Foundation'
  s.libraries = 'c++'
  s.pod_target_xcconfig = { 'OTHER_LDFLAGS' => '-ObjC', 'CLANG_CXX_LANGUAGE_STANDARD' => 'c++17' }
end
```

## Using the library from code
The whole API, the safety rules and the recipes are in the main `README.md` (sections 6, 7 and 9). The short version:

- Put `AudioConfig` values in, `Validate()` them, give `Recorder` and `Player` (or `ConferencePlayer`) the config, give the recorder a
  packet handler that sends datagrams, and give the player every datagram that arrives.
- Echo cancellation: create one `AudioProcessor`, `AudioProcessor::IsAvailable()` is true in this library; give it to the recorder
  and to the player (before `Player::SetConfig`). Use it when the speaker is on (no headphones).
- Calls, Siri and other apps taking the audio session are reported as `MicEvent::Interrupted` (`MicReason::SystemInterruption`)
  and `MicEvent::Free` afterwards; the speaker is re-opened by `Player::KeepDevice()` (it runs by itself) and the microphone by
  `AudioConfig::automaticResumeAfterFreeMic` (default on). A missed "interruption ended" expires after 2 minutes.

## What was tested
- CI: the library builds for device and simulator and a small program links against each slice.
- The iPhone demo (`build-demo/ios/`, built by `.github/workflows/build-demo-ios.yml` from this very xcframework) runs on an iPhone.
- Not available: Intel-Mac simulator (x86_64), bitcode (removed by Apple), macOS Catalyst, tvOS / watchOS.

## If the build fails
- `Undefined symbols ... OBJC_CLASS ...` or `... AVAudioSession ...`: a framework from the table or `-ObjC` is missing.
- `'ravava/Recorder.h' file not found`: the xcframework is not in the target's *Frameworks, Libraries*; or the file is plain C / ObjC.
  Use a `.mm` file.
- `Building for iOS Simulator, but the linked library was built for iOS`: you are using an old `.a`; use the xcframework.
- `library not found for -lc++`: set the C++ standard library to `libc++`.
- Crash on the first microphone use: `NSMicrophoneUsageDescription` is missing in `Info.plist`.
- Echo: with the speaker on and no headphones, create the `AudioProcessor` as above.

## How it is built
`.github/workflows/build-static-library-ios.yml` runs one job per SDK (`iphoneos`, `iphonesimulator`): it builds Opus 1.5.2 and
webrtc-audio-processing 1.3 (`libs/webrtc/build_ios.sh`, cached), builds RavAva with CMake, merges the three into one `libRavAva.a`
(`libtool -static`), links a small test program, and a last job packs both slices with `xcodebuild -create-xcframework`.
If the webrtc step of a slice fails, that slice is built without echo cancellation and the workflow shows a warning.

## Licenses

`licenses/` next to `RavAva.xcframework` holds the license texts of everything inside `libRavAva.a` (Opus, webrtc-audio-processing,
abseil, …) and `THIRD_PARTY_NOTICES.md`. An app that ships this library must show / include them (BSD-3-Clause and Apache-2.0
require the text to travel with the binary), e.g. in an in-app "Open source licenses" screen.
