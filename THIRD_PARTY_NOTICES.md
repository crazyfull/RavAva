# Third-party notices

RavAva itself is proprietary (see `LICENSE`). It links or bundles the components below, which keep their own licenses.
If you redistribute a binary built from RavAva you must also ship these notices and the license texts. The CI packages
(`RavAva-iOS-static`, `RavAva-Android-static`) already carry them in `licenses/` (texts copied from the exact sources that were compiled);
for your own builds run `libs/collect_licenses.sh <source dir> <out dir> <name>` on each dependency's source tree.

| Component | Used for | License | Where |
|---|---|---|---|
| [Opus](https://opus-codec.org) (libopus, incl. DRED) | audio codec | BSD-3-Clause | fetched by CMake (`FetchContent`) or `libs/opus/win32` (prebuilt, Windows/MSVC) |
| [miniaudio](https://miniaud.io) | device I/O | public domain (Unlicense) or MIT-0 | `miniaudio/` |
| [webrtc-audio-processing 1.3](https://gitlab.freedesktop.org/pulseaudio/webrtc-audio-processing) | AEC / NS / AGC (optional) | BSD-3-Clause (WebRTC) | built by `libs/webrtc/*` scripts |
| [abseil-cpp](https://abseil.io) | dependency of webrtc-audio-processing | Apache-2.0 | built with webrtc |
| [PFFFT](https://bitbucket.org/jpommier/pffft) | dependency of webrtc-audio-processing | BSD-like (FFTPACK) | built with webrtc |

## Provenance of `libs/opus/win32`

`libopus_x64.lib` / `libopus_x86.lib` and `include/` are prebuilt binaries/headers of upstream Opus compiled with MSVC.
To remove any doubt about them, rebuild from the upstream tag with CMake (`-DOPUS_DRED=ON -DOPUS_BUILD_SHARED_LIBRARY=OFF`)
and replace the files; the library's CMake also works without them (it fetches and builds Opus from source).

SHA-256 of the files currently in the repository:

    b64cadda23cb679a0ce6bcd28b03075b1b45a52b0ebf13fa184a1e5133f1321d  libopus_x64.lib
    3ea4b789251ed6f3364be85d113c7900fd7f234e4107102d4a7c9e46737c53f2  libopus_x86.lib
