# Changelog

RavAva library (`src/ravava`). The version is the `project(RavAva VERSION ...)` line in `src/ravava/CMakeLists.txt`; it ends
up in `<ravava/Version.h>` (`RAVAVA_VERSION_STRING`) and in the CMake package (`find_package(RavAva 0.1)`).
Not yet released as a number: everything below is in 0.1.0.

## 0.1.0 (in development)

### Fixed
- `ConferencePlayer` copies the `AudioConfig` in `SetConfig()`: new talkers are built later (from `Play()`), when the caller's config may already be gone. (`Recorder::Start()` and `Player::SetConfig()` already copied / read it synchronously.)
- MTU warning now uses `AudioConfig::MaxDatagramEstimate()`: 2 x mean payload (VBR peaks, FEC) + DRED allowance + 3-byte audio header + 3-byte envelope, threshold 1200 bytes.
- `AudioConfig::useEchoCancel` is documented as deprecated and never read (examples and tests use their own flag).
- CI packages `RavAva-iOS-static` and `RavAva-Android-static` now contain `licenses/` (Opus, webrtc-audio-processing, abseil and other dependency license texts copied from the compiled sources, `THIRD_PARTY_NOTICES.md`, RavAva `LICENSE`).
- `Player` adaptive playout is now really ON by default (the header and README said so, the member initialiser was `false`).
- `AudioConfig::useEchoCancel` documented as a hint only (the library never reads it; AEC runs when an `AudioProcessor` is attached).
- iOS: after a phone call / Siri the audio did not come back. iOS leaves the AVAudioSession inactive after an interruption and miniaudio activates it only once (when its context is created), so the re-opened devices started but stayed silent. `platform/IosAudioSession.cpp` now activates the session when the interruption ends and before every device (re)open.

### Added
- `ravava::Bandwidth::{Narrowband..Fullband}` constants for `AudioConfig::maxBandwidth`; a warning when `Bitrate × frameMs` can produce datagrams over ~1200 bytes.
- `THIRD_PARTY_NOTICES.md` (licenses of Opus, miniaudio, webrtc, abseil, PFFFT; provenance and SHA-256 of `libs/opus/win32`).
- README: benchmark table re-measured legacy → adaptive; security, MTU, iOS-session and 32-bit AEC notes; clearer real-time-safety and threading wording.
- iOS demo: the Log tab shows the library's own log (devices, interruptions) and "Copy log" includes it.
- Android static library: `.github/workflows/build-static-library-android.yml` -> `RavAva-Android-static` (`lib/<abi>/libRavAva.a` with Opus and webrtc merged in, `include/`; armeabi-v7a and x86 without webrtc so Android 5 stays supported). The native demo builds against it (`-PravavaPrebuilt`). `src/ravava/CMakeLists.txt` finds `libwebrtc_apm.a` on Android and sets the miniaudio Android backends.
- iOS: a call / Siri / another app taking the audio session is reported like on Android (`MicEvent::Interrupted`, `MicReason::SystemInterruption`, then `Free`); the speaker is re-opened by `Player::KeepDevice()` after the interruption (`OsAudioInterrupted()`), the microphone by the `automaticResumeAfterFreeMic` logic. A missed "interruption ended" expires after 2 minutes.
- Android: `android::SetAudioFocusLost()`: the application forwards the audio focus (a call of another app: Telegram, WhatsApp...); while it is lost the recorder stops the microphone (`MicEvent::Interrupted`, `MicReason::OtherApp`) and reports `Free` when it is back.
- Android 5 / 5.1: device lists and routes without `AudioManager.getDevices()` (API < 23); the active device is described from the route we set on OpenSL ES streams.
- `Player::SetDeviceLostHandler` / `Recorder::SetDeviceLostHandler`, `IsDeviceAvailable()`: when the chosen speaker / microphone vanishes (a Bluetooth headset switched off) the library switches to the system default device instead of re-opening the lost one in a loop, and reports the lost id.
- Microphone events: `Recorder::SetMicEventHandler` reports `Busy` (Start could not get the microphone), `Interrupted` (a phone call / another app / an iOS interruption took it; the recorder stops it) and `Free`. Free comes from the OS where it can tell (Android 7+, iOS), elsewhere from a test open every second. `AudioConfig::automaticResumeAfterFreeMic` (default true) restarts the microphone 2 s after Free, otherwise call `Recorder::ResumeMic()`. `Recorder::GetMicState()`.
- Android: no stream re-opening while the phone rings / is in a call; after a call in the media route the leftover SCO / communication device is cleared.
- Standalone CMake build of the library (static by default, shared option), `RavAva::RavAva` package, headers installed to
  `include/ravava/` with the Opus headers next to them; optional webrtc-audio-processing (echo cancellation, noise
  suppression, AGC); `RAVAVA_OPUS_ROOT` / `RAVAVA_OPUS_INCLUDE_DIR` / `RAVAVA_OPUS_LIB`.
- Loopback command line example (`example/loopback`), built for macOS (arm64 + Intel) by GitHub Actions.
- iOS static build (`.github/workflows/build-static-library-ios.yml` -> `RavAva.xcframework`: device arm64 + simulator arm64, each slice one `libRavAva.a` with Opus and webrtc merged in, headers included; replaces `build-ios.yml`): miniaudio as Objective-C, one shared
  play-and-record audio session, speaker + Bluetooth. Built and linked in CI, not yet run on an iPhone.
- `OpusCodec::EncodeFloat` / `DecodeFloat` and 24 / 32 bit sample converters (float / 24-bit PCM straight into Opus).
- `MusicPlayer` (play, pause, resume, stop, local monitor with its own volume), streamed from a background decoder thread.
- `ConferencePlayer`: several talkers into one speaker, routed by stream id.
- Adaptive playout (late-frame wait, trim / stretch), DRED playback (Opus 1.5+), jitter buffer sized from measured jitter.
- Level meters (`GetLevel`, `TakePeakLevel`), input gain, Packet header with sequence / marker.
- Android: call model like WhatsApp (`MODE_IN_COMMUNICATION`, communication device, Bluetooth SCO, speaker), recovery
  after calls and route changes, stalled-stream detection.

### Changed
- Low-cut (webrtc high-pass filter, below ~100 Hz) is a setting of its own: `AudioConfig::highPassFilter`, a check box in the demo,
  `--hpf` in the loopback example (off by default). It no longer needs the echo canceller / noise suppression to be switched on.
- Android: Bluetooth headphones are listed twice: "(Bluetooth handsfree)" = call quality with the headset's microphone, and
  "(Bluetooth high quality, phone microphone)" = A2DP media, no call mode, the phone's own microphone.
- Android: `AudioConfig::androidCallMode = false` runs without call mode (plain media streams), so the phone's own notification
  sounds keep working. Default stays on.
- Microphone and speaker are opened on a device worker (no UI freeze, no multi-second Start on Android).
- `Player` accepts packets even if the first speaker open failed (no silent playback after Stop / Start).
- Linux: a speaker "monitor" is never used as a microphone; "no microphone" is an error, not noise.

### Fixed
- Singing / speaking along with the music while "hear music" (the local monitor) is on: the listeners heard the voice late against the
  music, because the singer follows the delayed local sound. The SENT music is now delayed by the same time
  (`MusicPlayer::SetStreamDelayMs`, automatic by default; set it by ear on Bluetooth).
- Crashes while switching the microphone / speaker during a call: one lock around device open / close / listing,
  `DeviceSwitcher::Busy()` instead of a flag with a gap, the chosen device id is no longer read and written by two threads
  at once.
- Android: no more restore loops after a phone call, fresh Bluetooth microphone id for every open.
