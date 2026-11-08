#!/usr/bin/env bash
# Builds webrtc-audio-processing 1.3 (+ abseil) as ONE static library for iOS (arm64) with Xcode's clang (meson cross build):
#     <out>/lib/libwebrtc-audio-processing-1.a     the library and every abseil lib it needs, merged
#     <out>/include/                               webrtc headers (their own folder layout) + absl/
# Same layout as libs/webrtc/build_android.sh, so libRavAva's CMake finds it with -DRAVAVA_WEBRTC_ROOT=<out>.
#
# usage:  libs/webrtc/build_ios.sh <webrtc-audio-processing-1.3 source dir> <sdk: iphoneos|iphonesimulator> <out dir>
#         optional: IOS_MIN (default 14.0)
# Needs: a Mac (macOS runner), Xcode, meson + ninja (brew install meson ninja), internet (meson downloads abseil).
set -euo pipefail

SRC="$(cd "${1:?webrtc-audio-processing source dir}" && pwd)"
SDK="${2:-iphoneos}"
mkdir -p "${3:?output dir}"; OUT="$(cd "$3" && pwd)"
MIN="${IOS_MIN:-14.0}"

[ -f "$SRC/meson.build" ] || { echo "[error] $SRC/meson.build not found"; exit 1; }
SYSROOT="$(xcrun --sdk "$SDK" --show-sdk-path)"
CC="$(xcrun --sdk "$SDK" -f clang)"
CXX="$(xcrun --sdk "$SDK" -f clang++)"
AR="$(xcrun --sdk "$SDK" -f ar)"
RANLIB="$(xcrun --sdk "$SDK" -f ranlib)"
STRIP="$(xcrun --sdk "$SDK" -f strip)"
case "$SDK" in
    iphoneos)        VMIN="-miphoneos-version-min=$MIN" ;;
    iphonesimulator) VMIN="-mios-simulator-version-min=$MIN" ;;
    *) echo "[error] unknown sdk: $SDK"; exit 1 ;;
esac
FLAGS="'-arch', 'arm64', '-isysroot', '$SYSROOT', '$VMIN'"

WORK="$OUT/_build"
rm -rf "$WORK"; mkdir -p "$WORK" "$OUT/lib" "$OUT/include"

cat > "$WORK/cross.ini" <<INI
[binaries]
c = ['$CC']
cpp = ['$CXX']
ar = '$AR'
ranlib = '$RANLIB'
strip = '$STRIP'

[built-in options]
c_args = [$FLAGS]
cpp_args = [$FLAGS]
c_link_args = [$FLAGS]
cpp_link_args = [$FLAGS]

[host_machine]
system = 'darwin'
cpu_family = 'aarch64'
cpu = 'arm64'
endian = 'little'
INI

meson setup "$WORK/b" "$SRC" --cross-file "$WORK/cross.ini" --buildtype=release --default-library=static \
      || { echo "---- meson-log.txt ----"; tail -n 80 "$WORK/b/meson-logs/meson-log.txt" || true; exit 1; }
meson compile -C "$WORK/b"

MAIN="$(find "$WORK/b" -name 'libwebrtc-audio-processing-1.a' | head -n1)"
[ -n "$MAIN" ] || { echo "[error] libwebrtc-audio-processing-1.a not built"; find "$WORK/b" -name '*.a'; exit 1; }
# one archive: the main library + every abseil library (static libs of meson subprojects are not installed)
ALL=("$MAIN")
while read -r f; do ALL+=("$f"); done < <(find "$WORK/b/subprojects" -name '*.a' | sort)
libtool -static -o "$OUT/lib/libwebrtc-audio-processing-1.a" "${ALL[@]}"

# headers: webrtc ones keep their folder layout (modules/audio_processing/include/...), abseil under absl/
(cd "$SRC/webrtc" && find . -name '*.h' -print0 | xargs -0 -I{} install -D -m 0644 {} "$OUT/include/{}") 2>/dev/null || \
(cd "$SRC/webrtc" && find . -name '*.h' | while read -r h; do mkdir -p "$OUT/include/$(dirname "$h")"; cp "$h" "$OUT/include/$h"; done)
ABSL="$(find "$SRC/subprojects" -maxdepth 1 -type d -name 'abseil-cpp-*' | head -n1)"
[ -n "$ABSL" ] || { echo "[error] abseil sources not found under $SRC/subprojects"; exit 1; }
(cd "$ABSL" && find absl \( -name '*.h' -o -name '*.inc' \) | while read -r h; do mkdir -p "$OUT/include/$(dirname "$h")"; cp "$h" "$OUT/include/$h"; done)

rm -rf "$WORK"
echo "Done: $OUT"
lipo -info "$OUT/lib/libwebrtc-audio-processing-1.a"
ls -l "$OUT/lib"
