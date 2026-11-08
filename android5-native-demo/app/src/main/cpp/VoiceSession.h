/*
 * Full-duplex voice session of the native Android demo: the same pipeline as the Qt demo (Recorder -> packet envelope ->
 * UDP server or loopback -> PacketRouter -> ConferencePlayer), every setting of the Qt window, Qt-free.
 *
 * Threads: JNI calls come from the Java main thread (serialized by native-lib.cpp), the packet handler runs on the audio
 * thread, one thread receives UDP, one keeps the devices (KeepDevice ~1 Hz) and builds the statistics text.
 */
#pragma once

#include <jni.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <ravava/Recorder.h>
#include <ravava/ConferencePlayer.h>
#include <ravava/AudioConfig.h>
#include <ravava/AudioProcessor.h>
#include <ravava/net/Packet.h>
#include <ravava/net/PacketRouter.h>

struct SessionSettings {
    bool loopback = true;
    std::string server = "127.0.0.1";
    int port = 10001;
    int bitrate = 64000;
    int bandwidth = 1105;       // OPUS_BANDWIDTH_FULLBAND
    int frameMs = 20;
    int lossPct = 20;
    int channels = 1;
    bool agc = false, callMode = true, adaptive = false, hpf = false, dtx = true, musicMode = false;
    int noise = 0;              // 0 off .. 4 very high
    int echo = 0;               // 0 off, 1 default, 2..4 protect voice 1..3 (as the Qt window)
    int volume = 100, micGain = 100;
    bool talk = true;
    int musicVolume = 100;
    bool monitor = false;
    int monitorVolume = 100;
    std::string inputDevice, outputDevice;      // "" = system default
};

class VoiceSession {
public:
    VoiceSession();
    ~VoiceSession();

    // Call once from Java after obtaining the Application context.
    static bool InitAndroid(JavaVM *vm, jobject context);
    static bool WebrtcAvailable();

    bool Start(const SessionSettings &s);
    void Stop();
    bool Running() const { return running_; }

    // live changes (any time while running)
    void SetVolume(int v);
    void SetMicGain(int v);
    void SetTalk(bool on);
    void SetMusicVolume(int v);
    void SetMonitor(bool on);
    void SetMonitorVolume(int v);
    void SetAdaptive(bool on);
    void SetInputDevice(const std::string &id);
    void SetOutputDevice(const std::string &id);
    void SetNetwork(bool loopback, const std::string &server, int port);
    // music: 0 play (path), 1 pause, 2 resume, 3 stop. State: 0 stopped, 1 playing, 2 paused, 3 finished
    void Music(int op, const std::string &path);
    int MusicState() const;

    int MicLevel() const;   // 0..100
    int SpkLevel() const;   // 0..100
    std::string StatsText() const;      // the "log" tab, refreshed once a second
    std::string RateText() const;       // "Send: .. /sec  Receive: .. /sec", refreshed every 5 s

private:
    void udpReceiveLoop();
    void keepLoop();
    void buildStats();
    void onAudioPacket(const ravava::Packet::Header &hdr, const uint8_t *payload, size_t size);
    void sendToNetwork(const ravava::Bytes &body);

    ravava::AudioConfig cfg_;
    std::shared_ptr<ravava::AudioProcessor> aec_;
    std::unique_ptr<ravava::Recorder> rec_;
    std::unique_ptr<ravava::ConferencePlayer> play_;
    ravava::PacketRouter router_;
    std::mutex routerMu_;               // the router is not thread safe: audio thread (loopback) and the UDP thread
    uint16_t txStreamId_ = 0;
    uint16_t rxStreamId_ = 0;
    std::atomic<int64_t> rxStreamTime_{0};

    std::atomic<bool> running_{false};
    std::atomic<bool> loopback_{true};
    std::atomic<uint32_t> destAddr_{0};     // network byte order
    std::atomic<uint16_t> destPort_{0};     // host order
    int sock_ = -1;
    std::thread rxThread_, keepThread_;
    ravava::Bytes wire_;                    // audio thread only

    std::atomic<uint64_t> txBytes_{0}, rxBytes_{0};
    mutable std::mutex statsMu_;
    std::string statsText_ = "Idle", rateText_;
    struct Snap { uint32_t received, lostEvents, reordered; };
    std::vector<Snap> history_;
    uint64_t lastTx_ = 0, lastRx_ = 0;
    int64_t lastRateMs_ = 0;
};
