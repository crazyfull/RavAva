#include "VoiceSession.h"

#include <android/log.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <chrono>

#include <ravava/platform/AndroidAudioRoute.h>

#define LOG_TAG "ravava"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace ravava;

// ---------------------------------------------------------------------------
VoiceSession::VoiceSession() = default;

VoiceSession::~VoiceSession() {
    Stop();
}

bool VoiceSession::InitAndroid(JavaVM *vm, jobject context) {
    if (!vm || !context) return false;
    android::Init(vm, context);
    LOGI("ravava::android::Init done");
    return true;
}

// ---------------------------------------------------------------------------
bool VoiceSession::Start(bool udpMode, int localPort,
                         const std::string &remoteIp, int remotePort)
{
    if (running_) return true;

    udpMode_   = udpMode;
    remoteIp_  = remoteIp;
    remotePort_ = remotePort;

    // Fixed voice settings (same as the CLI loopback example)
    cfg_ = AudioConfig{};
    cfg_.inputRate   = 48000;
    cfg_.outputRate  = 48000;
    cfg_.channels    = 1;
    cfg_.Bitrate     = 64000;
    cfg_.frameMs     = 20;
    cfg_.maxBandwidth = 1105;          // fullband
    cfg_.useFec      = true;
    cfg_.androidCallMode = true;       // communication mode (WhatsApp-like)

    std::string err;
    if (!cfg_.Validate(&err)) {
        LOGE("AudioConfig::Validate failed: %s", err.c_str());
        lastError_ = err;
        return false;
    }

    rec_  = std::make_unique<Recorder>();
    play_ = std::make_unique<Player>();

    // Packet handler runs on the AUDIO thread → must be fast, no blocking.
    if (udpMode_) {
        // Create UDP socket
        sock_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_ < 0) {
            LOGE("socket() failed");
            return false;
        }
        int yes = 1;
        setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = INADDR_ANY;
        local.sin_port = htons(static_cast<uint16_t>(localPort));
        if (bind(sock_, reinterpret_cast<sockaddr*>(&local), sizeof(local)) < 0) {
            LOGE("bind() to port %d failed", localPort);
            ::close(sock_);
            sock_ = -1;
            return false;
        }
        LOGI("UDP listening on port %d, remote %s:%d", localPort, remoteIp_.c_str(), remotePort_);

        // Send path: Recorder → wrap with net::Packet → UDP
        rec_->SetPacketHandler([this](const Bytes &body) {
            // body is the 3-byte VoicePacket header + Opus
            // Optional network envelope: [type:1][streamId:2 LE][body]
            uint8_t buf[1300];
            size_t n = 0;
            buf[n++] = 1;                       // type = audio
            buf[n++] = 0; buf[n++] = 0;         // streamId = 0
            if (body.size() + n > sizeof(buf)) return;
            std::memcpy(buf + n, body.data(), body.size());
            n += body.size();

            sockaddr_in dest{};
            dest.sin_family = AF_INET;
            dest.sin_port   = htons(static_cast<uint16_t>(remotePort_));
            inet_pton(AF_INET, remoteIp_.c_str(), &dest.sin_addr);
            ::sendto(sock_, buf, n, 0,
                     reinterpret_cast<sockaddr*>(&dest), sizeof(dest));
        });
    } else {
        // Pure loopback
        rec_->SetPacketHandler([this](const Bytes &body) {
            play_->Play(body);
        });
    }

    // Open devices (SetConfig opens the speaker; Start opens the mic)
    play_->SetConfig(&cfg_);
    rec_->SetConfig(&cfg_);
    rec_->SetMicEventHandler([](ravava::MicEvent e, ravava::MicReason) {
        // Start() returns at once (the microphone opens on a worker): a busy / taken microphone is reported as an event
        LOGE("microphone event %d (0 busy, 1 interrupted, 2 free)", int(e));
    });
    rec_->Start();

    running_ = true;

    // Receive thread (UDP only)
    if (udpMode_) {
        rxThread_ = std::thread([this]{ udpReceiveLoop(); });
    }

    // KeepDevice watchdog (~1 Hz)
    keepThread_ = std::thread([this]{ keepDeviceLoop(); });

    LOGI("VoiceSession started (%s)", udpMode_ ? "UDP" : "loopback");
    return true;
}

// ---------------------------------------------------------------------------
void VoiceSession::Stop()
{
    if (!running_) return;
    running_ = false;

    if (rec_) {
        rec_->Stop();
    }

    if (rxThread_.joinable())  rxThread_.join();
    if (keepThread_.joinable()) keepThread_.join();

    if (sock_ >= 0) {
        ::close(sock_);
        sock_ = -1;
    }

    // Destroy in reverse order of creation (Safety Rules)
    rec_.reset();
    play_.reset();

    LOGI("VoiceSession stopped");
}

// ---------------------------------------------------------------------------
void VoiceSession::udpReceiveLoop()
{
    uint8_t buf[1500];
    while (running_) {
        sockaddr_in from{};
        socklen_t fromLen = sizeof(from);
        ssize_t n = ::recvfrom(sock_, buf, sizeof(buf), 0,
                               reinterpret_cast<sockaddr*>(&from), &fromLen);
        if (n < 4) continue;               // need at least type+streamId+something

        // Strip network envelope: [type:1][streamId:2][audio body]
        if (buf[0] != 1) continue;         // only audio
        const uint8_t *body = buf + 3;
        size_t bodyLen = static_cast<size_t>(n - 3);

        // Player::Play is thread-safe
        play_->Play(Bytes(body, body + bodyLen));
    }
}

void VoiceSession::keepDeviceLoop()
{
    while (running_) {
        if (rec_)  rec_->KeepDevice();
        if (play_) play_->KeepDevice();
        std::this_thread::sleep_for(std::chrono::milliseconds(900));
    }
}

// ---------------------------------------------------------------------------
int VoiceSession::MicLevel() const
{
    return rec_ ? rec_->GetLevel() : 0;
}

int VoiceSession::SpkLevel() const
{
    return play_ ? play_->GetLevel() : 0;
}

std::string VoiceSession::StatsText() const
{
    if (!running_ || !play_) return "Idle";

    JitterBuffer::Stats st;
    int depth = 0, frameMs = 0;
    if (!play_->GetStats(st, depth, frameMs)) {
        return "Running (no stats yet)";
    }

    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "recv %u  lost %u  fec %u  plc %u\n"
        "late %u  reorder %u  underrun %u\n"
        "buf %d ms",
        st.received, st.lost, st.fec, st.plc,
        st.late, st.reordered, st.underrun,
        depth * frameMs);
    return buf;
}
