/*
 * Simple full-duplex voice session for the native Android demo.
 * Supports pure loopback and optional UDP send/receive.
 *
 * Follows RavAva Safety Rules (lifetime, thread rules, KeepDevice, etc.).
 */
#pragma once

#include <jni.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <ravava/Recorder.h>
#include <ravava/Player.h>
#include <ravava/AudioConfig.h>
#include <ravava/net/Packet.h>

class VoiceSession {
public:
    VoiceSession();
    ~VoiceSession();

    // Call once from Java after obtaining the Application context.
    static bool InitAndroid(JavaVM *vm, jobject context);

    bool Start(bool udpMode, int localPort, const std::string &remoteIp, int remotePort);
    void Stop();

    int MicLevel() const;   // 0..100
    int SpkLevel() const;   // 0..100
    std::string StatsText() const;

private:
    void udpReceiveLoop();
    void keepDeviceLoop();

    ravava::AudioConfig cfg_;
    std::unique_ptr<ravava::Recorder> rec_;
    std::unique_ptr<ravava::Player>   play_;

    std::atomic<bool> running_{false};
    bool udpMode_ = false;

    // UDP
    int sock_ = -1;
    std::thread rxThread_;
    std::string remoteIp_;
    int remotePort_ = 0;

    // KeepDevice watchdog
    std::thread keepThread_;

    mutable std::mutex statsMu_;
    std::string lastError_;
};
