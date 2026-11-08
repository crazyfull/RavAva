#include "VoiceSession.h"

#include <android/log.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <chrono>
#include <cstdio>
#include <cstring>

#include <ravava/platform/AndroidAudioRoute.h>

#define LOG_TAG "ravava"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace ravava;

static int64_t nowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

VoiceSession::VoiceSession() = default;
VoiceSession::~VoiceSession() { Stop(); }

bool VoiceSession::InitAndroid(JavaVM *vm, jobject context)
{
    if (!vm || !context) return false;
    android::Init(vm, context);
    LOGI("ravava::android::Init done");
    return true;
}

bool VoiceSession::WebrtcAvailable() { return AudioProcessor::IsAvailable(); }

// ---------------------------------------------------------------------------
bool VoiceSession::Start(const SessionSettings &s)
{
    if (running_) return true;

    cfg_ = AudioConfig{};
    cfg_.inputRate  = 48000;
    cfg_.outputRate = 48000;
    cfg_.channels   = s.channels == 2 ? 2 : 1;
    cfg_.Bitrate    = s.bitrate;
    cfg_.frameMs    = s.frameMs;
    cfg_.maxBandwidth = s.bandwidth;
    cfg_.expectedLossPerc = s.lossPct;
    cfg_.useDtx     = s.dtx;
    cfg_.musicMode  = s.musicMode;
    cfg_.playMusic  = false;            // the music buttons mix the file into the stream at run time
    cfg_.useEchoCancel = s.echo != 0;
    cfg_.aecNearendProtection = s.echo;
    cfg_.noiseSuppression = s.noise;
    cfg_.autoGain   = s.agc;
    cfg_.highPassFilter = s.hpf;
    cfg_.androidCallMode = s.callMode;
    cfg_.inputDeviceId  = s.inputDevice;
    cfg_.outputDeviceId = s.outputDevice;

    std::string err;
    if (!cfg_.Validate(&err)) {
        LOGE("AudioConfig::Validate failed: %s", err.c_str());
        return false;
    }

    // ---- network: one UDP socket (not bound: the OS picks the port, the server answers to it), like the Qt demo
    sock_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_ < 0) { LOGE("socket() failed"); return false; }
    timeval tv{0, 200 * 1000};                  // so the receive thread notices Stop()
    setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    SetNetwork(s.loopback, s.server, s.port);

    txStreamId_ = Packet::NewStreamId();
    rxStreamId_ = 0;
    rxStreamTime_ = 0;
    txBytes_ = rxBytes_ = 0;
    lastTx_ = lastRx_ = 0;
    lastRateMs_ = nowMs();
    {
        std::lock_guard<std::mutex> lk(statsMu_);
        history_.clear();
        statsText_ = "Starting...";
        rateText_.clear();
    }

    router_ = PacketRouter();
    router_.SetHandler(Packet::Audio, [this](const Packet::Header &hdr, const uint8_t *payload, size_t size) {
        onAudioPacket(hdr, payload, size);
    });

    // ---- audio objects, in the order the Qt demo creates them
    rec_  = std::make_unique<Recorder>();
    play_ = std::make_unique<ConferencePlayer>();
    play_->SetVolume(s.volume);
    play_->SetAdaptivePlayout(s.adaptive);
    rec_->SetInputGain(s.micGain);
    rec_->Music()->SetVolume(s.musicVolume);
    rec_->Music()->SetMonitorVolume(s.monitorVolume);
    rec_->Music()->SetMonitor(s.monitor);
    play_->SetMusicMonitor(rec_->Music());

    aec_.reset();
    const bool wantAec = cfg_.useEchoCancel;
    if (AudioProcessor::IsAvailable() && (wantAec || cfg_.noiseSuppression > 0 || cfg_.autoGain || cfg_.highPassFilter)) {
        auto aec = std::make_shared<AudioProcessor>();
        AudioProcessor::Options opt;
        opt.streamDelayMs = cfg_.aecDelayMs;
        opt.nearendProtection = cfg_.aecNearendProtection;
        opt.echoCancel = wantAec;
        opt.noiseSuppression = cfg_.noiseSuppression;
        opt.autoGain = cfg_.autoGain;
        opt.highPassFilter = cfg_.highPassFilter || wantAec;
        std::string e2;
        if (aec->Init(cfg_.inputRate, cfg_.RenderRate(), cfg_.channels, opt, &e2)) aec_ = aec;
        else LOGE("echo canceller disabled: %s", e2.c_str());
    }
    rec_->SetAudioProcessor(aec_);
    play_->SetAudioProcessor(aec_);             // before SetConfig: that starts the speaker

    // the recorder's packet handler runs on the AUDIO thread: wrap, then send (or loop back), never block
    rec_->SetPacketHandler([this](const Bytes &body) { sendToNetwork(body); });
    rec_->SetMicEventHandler([](MicEvent e, MicReason r) {
        LOGI("microphone event: %s (reason %d)", e == MicEvent::Busy ? "busy, cannot start" : e == MicEvent::Interrupted ? "interrupted" : "free", int(r));
    });
    play_->SetConfig(&cfg_);
    rec_->SetConfig(&cfg_);
    rec_->SetMicEnabled(s.talk);

    running_ = true;
    rxThread_   = std::thread([this] { udpReceiveLoop(); });
    keepThread_ = std::thread([this] { keepLoop(); });
    rec_->Start();                              // the microphone opens on a worker thread

    LOGI("VoiceSession started (%s), stream id %u", s.loopback ? "loopback" : "UDP", unsigned(txStreamId_));
    return true;
}

void VoiceSession::Stop()
{
    if (!running_) return;
    running_ = false;

    if (rec_) rec_->Stop();                     // also stops the music
    if (rxThread_.joinable())   rxThread_.join();
    if (keepThread_.joinable()) keepThread_.join();
    if (sock_ >= 0) { ::close(sock_); sock_ = -1; }

    rec_.reset();                               // reverse order of creation
    play_.reset();
    aec_.reset();
    std::lock_guard<std::mutex> lk(statsMu_);
    statsText_ = "Idle";
    LOGI("VoiceSession stopped");
}

// ---------------------------------------------------------------------------
void VoiceSession::sendToNetwork(const Bytes &body)
{
    Packet::Header hdr;
    hdr.type = Packet::Audio;
    hdr.streamId = txStreamId_;
    Packet::WrapInto(wire_, hdr, body);
    if (loopback_.load(std::memory_order_relaxed)) {
        txBytes_ += wire_.size();               // "sent" and "received" at once
        rxBytes_ += wire_.size();
        std::lock_guard<std::mutex> lk(routerMu_);
        router_.Dispatch(wire_);
        return;
    }
    const uint32_t addr = destAddr_.load(std::memory_order_relaxed);
    if (!addr || sock_ < 0) return;
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = addr;
    dest.sin_port = htons(destPort_.load(std::memory_order_relaxed));
    const ssize_t n = ::sendto(sock_, wire_.data(), wire_.size(), MSG_DONTWAIT, reinterpret_cast<sockaddr *>(&dest), sizeof(dest));
    if (n > 0) txBytes_ += size_t(n);
}

void VoiceSession::onAudioPacket(const Packet::Header &hdr, const uint8_t *payload, size_t size)
{
    rxStreamId_ = hdr.streamId;
    rxStreamTime_ = nowMs();
    if (play_) play_->Play(hdr.streamId, payload, size);    // one jitter buffer + decoder per talker
}

void VoiceSession::udpReceiveLoop()
{
    uint8_t buf[2048];
    while (running_) {
        const int fd = sock_;
        if (fd < 0) break;
        const ssize_t n = ::recvfrom(fd, buf, sizeof(buf), 0, nullptr, nullptr);
        if (n <= 0) continue;                   // timeout (200 ms) or error: look at running_ again
        rxBytes_ += size_t(n);
        std::lock_guard<std::mutex> lk(routerMu_);
        router_.Dispatch(buf, size_t(n));
    }
}

void VoiceSession::keepLoop()
{
    int tick = 0;
    while (running_) {
        if (rec_)  rec_->KeepDevice();
        if (play_) play_->KeepDevice();
        buildStats();
        if (++tick % 5 == 0) {
            const int64_t t = nowMs();
            std::lock_guard<std::mutex> lk(statsMu_);
            const double secs = std::max<int64_t>(1, t - lastRateMs_) / 1000.0;
            auto fmt = [](double bytes) {
                char b[48];
                if (bytes >= 1e6) std::snprintf(b, sizeof(b), "%.3f MB", bytes / 1e6);
                else if (bytes >= 1e3) std::snprintf(b, sizeof(b), "%.2f KB", bytes / 1e3);
                else std::snprintf(b, sizeof(b), "%.0f bytes", bytes);
                return std::string(b);
            };
            rateText_ = "Send: " + fmt((txBytes_ - lastTx_) / secs) + "/sec    Receive: " + fmt((rxBytes_ - lastRx_) / secs) + "/sec";
            lastTx_ = txBytes_; lastRx_ = rxBytes_; lastRateMs_ = t;
        }
        for (int i = 0; i < 10 && running_; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// The "log" text: the same lines as the Qt demo (devices really in use, music state, receive statistics of the talker heard last).
void VoiceSession::buildStats()
{
    if (!rec_ || !play_) return;
    std::string devices;
    char line[512];
    {
        const std::string mic = rec_->ActiveInputDevice(), spk = play_->ActiveOutputDevice();
        std::snprintf(line, sizeof(line), "Mic      : %s\nSpeaker  : %s\n",
                      mic.empty() ? "(not running / unknown)" : mic.c_str(), spk.empty() ? "(not running / unknown)" : spk.c_str());
        devices = line;
    }
    using MS = ravava::MusicPlayer::State;
    const MS ms = rec_->Music()->GetState();
    if (ms != MS::Stopped) {
        const std::string merr = rec_->Music()->Error();
        devices += !merr.empty() ? "Music    : ERROR - " + merr + "\n"
                   : ms == MS::Finished ? "Music    : finished\n" : ms == MS::Paused ? "Music    : paused\n" : "Music    : playing\n";
    }

    JitterBuffer::Stats st;
    int depthFrames = 0, frameMs = 0;
    std::string text;
    const uint16_t rxId = rxStreamId_;
    if (!play_->GetStreamStats(rxId, st, depthFrames, frameMs)) {
        text = devices + "Receive stats: (not started, or no audio received yet)";
    } else {
        std::lock_guard<std::mutex> lk(statsMu_);
        auto pct = [](uint32_t lost, uint32_t recv) { const uint32_t total = recv + lost; return total ? 100.0 * lost / total : 0.0; };
        if (!history_.empty() && st.received < history_.back().received) history_.clear();     // a new stream format: counters restart
        history_.push_back({st.received, st.lostEvents, st.reordered});
        while (history_.size() > 6) history_.erase(history_.begin());
        const Snap old = history_.front();
        const uint32_t recv5 = st.received - old.received;
        const uint32_t newLost = st.lostEvents - old.lostEvents;
        const uint32_t newLate = st.reordered - old.reordered;
        const uint32_t lost5 = newLost > newLate ? newLost - newLate : 0;

        text = devices;
        std::snprintf(line, sizeof(line), "Received : %u packets\n", st.received); text += line;
        std::snprintf(line, sizeof(line), "Lost     : %u packets (%.1f%%)   last 5s: %u (%.1f%%)\n", st.lost, pct(st.lost, st.received), lost5, pct(lost5, recv5)); text += line;
        std::snprintf(line, sizeof(line), "Recovered: FEC %u   PLC %u   too late %u   reordered %u\n", st.fec, st.plc, st.late, st.reordered); text += line;
        std::snprintf(line, sizeof(line), "Buffer   : %d frames (%d ms)   underruns %u", depthFrames, depthFrames * frameMs, st.underrun); text += line;
        if (st.ingestDropped) { std::snprintf(line, sizeof(line), "   dropped before buffer %u", st.ingestDropped); text += line; }
        if (st.formatDropped) { std::snprintf(line, sizeof(line), "   not playable / other format %u", st.formatDropped); text += line; }

        AudioProcessor::Stats as;
        if (cfg_.useEchoCancel && aec_ && aec_->GetStats(as)) {
            if (as.hasDelay) std::snprintf(line, sizeof(line), "\nEcho     : cancelling, estimated echo delay %d ms", as.delayMs);
            else std::snprintf(line, sizeof(line), "\nEcho     : cancelling (still measuring the delay)");
            text += line;
        } else text += "\nEcho     : off";

        if (rxStreamTime_) {
            std::snprintf(line, sizeof(line), "\nStream   : stats of stream #%u (last heard %lld ms ago), you are #%u", unsigned(rxId), (long long)(nowMs() - rxStreamTime_), unsigned(txStreamId_));
            text += line;
        }
        const std::vector<uint16_t> talkers = play_->ActiveStreams();
        if (!talkers.empty()) {
            std::snprintf(line, sizeof(line), "\nTalkers  : %zu in the room:", talkers.size()); text += line;
            for (uint16_t id : talkers) { std::snprintf(line, sizeof(line), " #%u (level %d)", unsigned(id), play_->GetStreamLevel(id)); text += line; }
        }
        const PacketRouter::Stats &net = router_.GetStats();
        if (net.unhandled || net.malformed) {
            std::snprintf(line, sizeof(line), "\nNetwork  : ignored %u unknown type, %u malformed", unsigned(net.unhandled), unsigned(net.malformed)); text += line;
        }
        statsText_ = text;
        return;
    }
    std::lock_guard<std::mutex> lk(statsMu_);
    statsText_ = text;
}

// ---------------------------------------------------------------------------
void VoiceSession::SetVolume(int v)        { if (play_) play_->SetVolume(v); }
void VoiceSession::SetMicGain(int v)       { if (rec_) rec_->SetInputGain(v); }
void VoiceSession::SetTalk(bool on)        { if (rec_) rec_->SetMicEnabled(on); }
void VoiceSession::SetMusicVolume(int v)   { if (rec_) rec_->Music()->SetVolume(v); }
void VoiceSession::SetMonitor(bool on)     { if (rec_) rec_->Music()->SetMonitor(on); }
void VoiceSession::SetMonitorVolume(int v) { if (rec_) rec_->Music()->SetMonitorVolume(v); }
void VoiceSession::SetAdaptive(bool on)    { if (play_) play_->SetAdaptivePlayout(on); }
void VoiceSession::SetInputDevice(const std::string &id)  { if (rec_)  rec_->SetInputDevice(id); }
void VoiceSession::SetOutputDevice(const std::string &id) { if (play_) play_->SetOutputDevice(id); }

void VoiceSession::SetNetwork(bool loopback, const std::string &server, int port)
{
    uint32_t addr = 0;
    in_addr a{};
    if (inet_pton(AF_INET, server.c_str(), &a) == 1) {
        addr = a.s_addr;
    } else if (!server.empty()) {                // a host name (not the audio thread: this blocks)
        addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        if (getaddrinfo(server.c_str(), nullptr, &hints, &res) == 0 && res) {
            addr = reinterpret_cast<sockaddr_in *>(res->ai_addr)->sin_addr.s_addr;
            freeaddrinfo(res);
        } else LOGE("cannot resolve '%s'", server.c_str());
    }
    destPort_ = uint16_t(port);
    destAddr_ = addr;
    loopback_ = loopback;
}

void VoiceSession::Music(int op, const std::string &path)
{
    if (!rec_) return;
    switch (op) {
    case 0: rec_->Music()->Play(path); break;
    case 1: rec_->Music()->Pause(); break;
    case 2: rec_->Music()->Resume(); break;
    case 3: rec_->Music()->Stop(); break;
    }
}

int VoiceSession::MusicState() const
{
    using MS = ravava::MusicPlayer::State;
    if (!rec_) return 0;
    switch (rec_->Music()->GetState()) {
    case MS::Playing:  return 1;
    case MS::Paused:   return 2;
    case MS::Finished: return 3;
    default:           return 0;
    }
}

int VoiceSession::MicLevel() const { return rec_ ? rec_->GetLevel() : 0; }
int VoiceSession::SpkLevel() const { return play_ ? play_->GetLevel() : 0; }

std::string VoiceSession::StatsText() const { std::lock_guard<std::mutex> lk(statsMu_); return statsText_; }
std::string VoiceSession::RateText() const  { std::lock_guard<std::mutex> lk(statsMu_); return rateText_; }
