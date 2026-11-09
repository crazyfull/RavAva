cmake_minimum_required(VERSION 3.18)
project(ravava_demo LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Paths: build-demo/android-native/app/src/main/cpp → 6 levels up = repo root
# ---------------------------------------------------------------------------
set(RAVAVA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../..")
set(RAVAVA_SRC  "${RAVAVA_ROOT}/src/ravava")
set(MINIAUDIO   "${RAVAVA_ROOT}/miniaudio")

if(NOT EXISTS "${RAVAVA_SRC}/Recorder.h")
    message(FATAL_ERROR
        "Cannot find RavAva sources at ${RAVAVA_SRC}\n"
        "Expected layout: RavAva/build-demo/android-native/app/src/main/cpp/")
endif()

message(STATUS "RAVAVA_ROOT = ${RAVAVA_ROOT}")
message(STATUS "RAVAVA_SRC  = ${RAVAVA_SRC}")

# ---------------------------------------------------------------------------
# Opus (static, from source)
# CMake 3.22 (Android SDK) does not accept DOWNLOAD_EXTRACT_TIMESTAMP here.
# ---------------------------------------------------------------------------
include(FetchContent)
set(OPUS_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    opus
    URL https://downloads.xiph.org/releases/opus/opus-1.5.2.tar.gz
    URL_HASH SHA256=65c1d2f78b9f2fb20082c38cbe47c951ad5839345876e46941612ee87f9a7ce1
)
FetchContent_MakeAvailable(opus)

# ---------------------------------------------------------------------------
# libRavAva sources
# ---------------------------------------------------------------------------
set(RAVAVA_SOURCES
    ${RAVAVA_SRC}/AudioDevice.cpp
    ${RAVAVA_SRC}/AudioProcessor.cpp
    ${RAVAVA_SRC}/JitterBuffer.cpp
    ${RAVAVA_SRC}/MusicPlayer.cpp
    ${RAVAVA_SRC}/OpusCodec.cpp
    ${RAVAVA_SRC}/Player.cpp
    ${RAVAVA_SRC}/ConferencePlayer.cpp
    ${RAVAVA_SRC}/Recorder.cpp
    ${RAVAVA_SRC}/net/PacketRouter.cpp
    ${RAVAVA_SRC}/platform/AndroidAudioRoute.cpp
    ${MINIAUDIO}/miniaudio.c
)

add_library(ravava_static STATIC ${RAVAVA_SOURCES})
target_include_directories(ravava_static PUBLIC
    ${RAVAVA_SRC}
    ${RAVAVA_SRC}/..
    ${MINIAUDIO}
    ${opus_SOURCE_DIR}/include
)
target_compile_definitions(ravava_static PUBLIC
    RAVAVA_ANDROID=1
    MA_ENABLE_ONLY_SPECIFIC_BACKENDS=1
    MA_ENABLE_AAUDIO=1
    MA_ENABLE_OPENSL=1
)
target_link_libraries(ravava_static PUBLIC opus log android OpenSLES)

# webrtc-audio-processing (optional): -DWEBRTC_ROOT=<libs/webrtc/build_android.sh output for this ABI> (lib/libwebrtc_apm.a + include/)
set(WEBRTC_ROOT "" CACHE PATH "libs/webrtc/build_android.sh output (lib/libwebrtc_apm.a, include/)")
if(WEBRTC_ROOT AND EXISTS "${WEBRTC_ROOT}/lib/libwebrtc_apm.a" AND EXISTS "${WEBRTC_ROOT}/include/modules/audio_processing/include/audio_processing.h")
    message(STATUS "WebRTC audio processing: ON (${WEBRTC_ROOT})")
    target_compile_definitions(ravava_static PUBLIC RAVAVA_WITH_WEBRTC)
    target_include_directories(ravava_static PUBLIC "${WEBRTC_ROOT}/include")
    target_link_libraries(ravava_static PUBLIC "${WEBRTC_ROOT}/lib/libwebrtc_apm.a")
else()
    message(STATUS "WebRTC audio processing: OFF (no WEBRTC_ROOT): no echo cancellation / noise suppression / AGC")
endif()

# ---------------------------------------------------------------------------
# Demo shared library (JNI)
# ---------------------------------------------------------------------------
add_library(ravava_demo SHARED
    native-lib.cpp
    VoiceSession.cpp
)
target_include_directories(ravava_demo PRIVATE
    ${RAVAVA_SRC}
    ${RAVAVA_SRC}/..
)
target_link_libraries(ravava_demo
    ravava_static
    log
    android
)
