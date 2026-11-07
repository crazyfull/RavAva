// Microphone events: Busy at Start, Interrupted while recording, Free (from the OS, or by probing), automatic resume after 2 s.
// Fake devices (tests/stub/audio_device_stub.cpp), no sound card. Build line: tests/README.txt
#include "ravava/Recorder.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

using namespace ravava;

static int g_fail = 0;
#define CHECK(c) do{ if(!(c)){ std::printf("  FAIL line %d: %s\n", __LINE__, #c); g_fail++; } }while(0)

extern std::atomic<bool> g_stubMicBusy, g_stubInterrupted;
extern std::atomic<int> g_stubMicOs;
extern std::atomic<bool> g_stubDeviceGone;
std::string StubInputDevice();

static std::mutex g_mx;
static std::vector<MicEvent> g_ev;
static int count(MicEvent e){ std::lock_guard<std::mutex> lk(g_mx); int n = 0; for(auto x : g_ev) n += x == e; return n; }
static bool waitFor(const std::function<bool()> &c, int ms)
{
    for(int i = 0; i < ms / 10 && !c(); i++) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return c();
}

int main()
{
    for(int autoResume = 1; autoResume >= 0; autoResume--){
        std::printf("automaticResumeAfterFreeMic = %d\n", autoResume);
        { std::lock_guard<std::mutex> lk(g_mx); g_ev.clear(); }
        g_stubMicBusy = false; g_stubInterrupted = false; g_stubMicOs = -1;
        AudioConfig cfg; cfg.automaticResumeAfterFreeMic = autoResume != 0;
        Recorder rec;
        rec.SetMicEventHandler([](MicEvent e, MicReason){ std::lock_guard<std::mutex> lk(g_mx); g_ev.push_back(e); });
        rec.SetConfig(&cfg);

        // 1. the microphone is busy at Start: Busy event, nothing runs
        g_stubMicBusy = true;
        rec.Start();
        CHECK(waitFor([&]{ return count(MicEvent::Busy) == 1; }, 4000));
        CHECK(rec.GetMicState() == MicState::Waiting);

        // 2. it becomes free (no OS signal: the recorder probes): Free event, and with auto resume it starts again after ~2 s
        g_stubMicBusy = false;
        CHECK(waitFor([&]{ return count(MicEvent::Free) == 1; }, 4000));
        if(autoResume){
            CHECK(waitFor([&]{ return rec.GetMicState() == MicState::Active; }, 5000));
        }else{
            std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            CHECK(rec.GetMicState() == MicState::Free);         // only the event, the application decides
            rec.ResumeMic();
            CHECK(waitFor([&]{ return rec.GetMicState() == MicState::Active; }, 2000));
        }
        CHECK(waitFor([&]{ return rec.GetLevel() > 0; }, 2000));    // sound flows again

        // 3. taken while recording (a call): Interrupted, stopped; the OS says "busy" then "free": Free
        g_stubMicOs = 1;
        g_stubInterrupted = true;
        CHECK(waitFor([&]{ return count(MicEvent::Interrupted) == 1; }, 4000));
        g_stubInterrupted = false;
        CHECK(rec.GetMicState() == MicState::Waiting);
        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
        CHECK(count(MicEvent::Free) == 1);                     // still busy: no new Free
        g_stubMicOs = 0;
        CHECK(waitFor([&]{ return count(MicEvent::Free) == 2; }, 4000));
        rec.Stop();
        CHECK(rec.GetMicState() == MicState::Off);
    }
    // the chosen microphone vanishes (headset off): the recorder goes to the system default and tells it once
    {
        std::printf("device lost\n");
        g_stubMicBusy = false; g_stubInterrupted = false; g_stubMicOs = -1; g_stubDeviceGone = false;
        AudioConfig cfg; cfg.inputDeviceId = "bb";
        Recorder rec;
        std::atomic<int> lost{0};
        std::string lostId; std::mutex m;
        rec.SetDeviceLostHandler([&](const std::string &id){ std::lock_guard<std::mutex> lk(m); lostId = id; lost++; });
        rec.SetConfig(&cfg);
        rec.Start();
        CHECK(waitFor([&]{ return StubInputDevice() == "bb"; }, 2000));
        g_stubDeviceGone = true;
        extern std::atomic<bool> g_stubInputMoved; g_stubInputMoved = true;      // the system moved the stream
        CHECK(waitFor([&]{ return lost == 1; }, 5000));
        { std::lock_guard<std::mutex> lk(m); CHECK(lostId == "bb"); }
        CHECK(waitFor([&]{ return StubInputDevice().empty(); }, 2000));        // opened on the system default now
        rec.Stop();
        g_stubDeviceGone = false;
    }
    std::printf(g_fail ? "FAIL (%d)\n" : "PASS\n", g_fail);
    return g_fail ? 1 : 0;
}
