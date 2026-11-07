cmake_minimum_required(VERSION 3.18)
project(ravava_demo LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Paths relative to this demo when placed inside crazyfull/RavAva/android4-demo
# (or android5-native-demo). Adjust if you put the folder elsewhere.
#
# From:  android4-demo/app/src/main/cpp/CMakeLists.txt
# Up 5 levels → repository root
# ---------------------------------------------------------------------------
set(RAVAVA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")
set(RAVAVA_SRC  "${RAVAVA_ROOT}/src/ravava")
set(MINIAUDIO   "${RAVAVA_ROOT}/miniaudio")

# Sanity check
if(NOT EXISTS "${RAVAVA_SRC}/Recorder.h")
    message(FATAL_ERROR
        "Cannot find RavAva sources at ${RAVAVA_SRC}\n"
        "This demo expects to live inside the RavAva repository, e.g.\n"
        "  crazyfull/RavAva/android4-demo/\n"
        "so that ../../../../.. reaches the repo root.")
endif()

# ---------------------------------------------------------------------------
# Opus (build from source, static)
# ---------------------------------------------------------------------------
include(FetchContent)
set(OPUS_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    opus
    URL https://downloads.xiph.org/releases/opus/opus-1.5.2.tar.gz
    URL_HASH SHA256=65c1d2f78b9f2fb20082c38cbe47c736e71833e31e200d618de5a4c2a7c5b8f7
)
FetchContent_MakeAvailable(opus)

# ---------------------------------------------------------------------------
# libRavAva sources (no WebRTC)
# ---------------------------------------------------------------------------
set(RAVAVA_SOURCES
    ${RAVAVA_SRC}/AudioDevice.cpp
    ${RAVAVA_SRC}/AudioProcessor.cpp
    ${RAVAVA_SRC}/JitterBuffer.cpp
    ${RAVAVA_SRC}/MusicPlayer.cpp
    ${RAVAVA_SRC}/OpusCodec.cpp
    ${RAVAVA_SRC}/Player.cpp
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
    MA_NO_RUNTIME_LINKING=1
    MA_ENABLE_ONLY_SPECIFIC_BACKENDS=1
    MA_ENABLE_AAUDIO=1
    MA_ENABLE_OPENSL=1
)
# OpenSLES is always present on API 21+; AAudio is loaded at runtime by miniaudio on API 26+
target_link_libraries(ravava_static PUBLIC opus log android OpenSLES)

# ---------------------------------------------------------------------------
# Demo shared library (JNI entry points)
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
