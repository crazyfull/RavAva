#include <jni.h>
#include <android/log.h>
#include <memory>
#include <mutex>
#include <string>

#include <ravava/AudioDevice.h>
#include "VoiceSession.h"

#define LOG_TAG "ravava"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)

namespace {
std::mutex g_mu;
std::unique_ptr<VoiceSession> g_session;
JavaVM *g_vm = nullptr;

std::string str(JNIEnv *env, jstring s)
{
    if (!s) return std::string();
    const char *c = env->GetStringUTFChars(s, nullptr);
    std::string out = c ? c : "";
    if (c) env->ReleaseStringUTFChars(s, c);
    return out;
}
}

#define JNI(ret, name) extern "C" JNIEXPORT ret JNICALL Java_com_ravava_demo_VoiceService_##name

JNI(jboolean, nativeInit)(JNIEnv *env, jclass, jobject context)
{
    std::lock_guard<std::mutex> lk(g_mu);
    if (!g_vm) env->GetJavaVM(&g_vm);
    jobject globalCtx = env->NewGlobalRef(context);     // kept for the lifetime of the process
    return VoiceSession::InitAndroid(g_vm, globalCtx) ? JNI_TRUE : JNI_FALSE;
}

JNI(jboolean, nativeWebrtc)(JNIEnv *, jclass) { return VoiceSession::WebrtcAvailable() ? JNI_TRUE : JNI_FALSE; }

// p = the integer settings, in this order (see VoiceService.java): loopback, bitrate, bandwidth, frameMs, lossPct, channels, agc,
// callMode, adaptive, hpf, dtx, musicMode, noise, echo, volume, micGain, talk, musicVolume, monitor, monitorVolume
JNI(jboolean, nativeStart)(JNIEnv *env, jclass, jintArray p, jstring server, jint port, jstring inDev, jstring outDev)
{
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_session) { g_session->Stop(); g_session.reset(); }
    jint v[20] = {0};
    env->GetIntArrayRegion(p, 0, 20, v);
    SessionSettings s;
    s.loopback = v[0]; s.bitrate = v[1]; s.bandwidth = v[2]; s.frameMs = v[3]; s.lossPct = v[4]; s.channels = v[5];
    s.agc = v[6]; s.callMode = v[7]; s.adaptive = v[8]; s.hpf = v[9]; s.dtx = v[10]; s.musicMode = v[11];
    s.noise = v[12]; s.echo = v[13]; s.volume = v[14]; s.micGain = v[15]; s.talk = v[16];
    s.musicVolume = v[17]; s.monitor = v[18]; s.monitorVolume = v[19];
    s.server = str(env, server); s.port = port;
    s.inputDevice = str(env, inDev); s.outputDevice = str(env, outDev);
    g_session = std::make_unique<VoiceSession>();
    if (!g_session->Start(s)) { g_session.reset(); return JNI_FALSE; }
    return JNI_TRUE;
}

JNI(void, nativeStop)(JNIEnv *, jclass)
{
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_session) { g_session->Stop(); g_session.reset(); }
}

// what: 0 volume, 1 mic gain, 2 talk, 3 music volume, 4 monitor, 5 monitor volume, 6 adaptive
JNI(void, nativeSet)(JNIEnv *, jclass, jint what, jint value)
{
    std::lock_guard<std::mutex> lk(g_mu);
    if (!g_session) return;
    switch (what) {
    case 0: g_session->SetVolume(value); break;
    case 1: g_session->SetMicGain(value); break;
    case 2: g_session->SetTalk(value != 0); break;
    case 3: g_session->SetMusicVolume(value); break;
    case 4: g_session->SetMonitor(value != 0); break;
    case 5: g_session->SetMonitorVolume(value); break;
    case 6: g_session->SetAdaptive(value != 0); break;
    }
}

JNI(void, nativeSetDevice)(JNIEnv *env, jclass, jboolean capture, jstring id)
{
    const std::string d = str(env, id);
    std::lock_guard<std::mutex> lk(g_mu);
    if (!g_session) return;
    if (capture) g_session->SetInputDevice(d); else g_session->SetOutputDevice(d);
}

JNI(void, nativeSetNetwork)(JNIEnv *env, jclass, jboolean loopback, jstring server, jint port)
{
    const std::string srv = str(env, server);
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_session) g_session->SetNetwork(loopback == JNI_TRUE, srv, port);
}

// op: 0 play (path), 1 pause, 2 resume, 3 stop
JNI(void, nativeMusic)(JNIEnv *env, jclass, jint op, jstring path)
{
    const std::string p = str(env, path);
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_session) g_session->Music(op, p);
}

JNI(jint, nativeMusicState)(JNIEnv *, jclass)
{
    std::lock_guard<std::mutex> lk(g_mu);
    return g_session ? g_session->MusicState() : 0;
}

// "name\n id \n default(0/1)" per device
JNI(jobjectArray, nativeListDevices)(JNIEnv *env, jclass, jboolean capture)
{
    const std::vector<ravava::AudioDeviceInfo> list = capture ? ravava::ListInputDevices() : ravava::ListOutputDevices();
    jclass strCls = env->FindClass("java/lang/String");
    jobjectArray arr = env->NewObjectArray(jsize(list.size()), strCls, nullptr);
    for (size_t i = 0; i < list.size(); ++i) {
        const std::string e = list[i].name + "\n" + list[i].id + "\n" + (list[i].isDefault ? "1" : "0");
        jstring js = env->NewStringUTF(e.c_str());
        env->SetObjectArrayElement(arr, jsize(i), js);
        env->DeleteLocalRef(js);
    }
    return arr;
}

JNI(jint, nativeMicLevel)(JNIEnv *, jclass)
{
    std::lock_guard<std::mutex> lk(g_mu);
    return g_session ? g_session->MicLevel() : 0;
}

JNI(jint, nativeSpkLevel)(JNIEnv *, jclass)
{
    std::lock_guard<std::mutex> lk(g_mu);
    return g_session ? g_session->SpkLevel() : 0;
}

JNI(jstring, nativeStats)(JNIEnv *env, jclass)
{
    std::lock_guard<std::mutex> lk(g_mu);
    return env->NewStringUTF(g_session ? g_session->StatsText().c_str() : "Idle");
}

JNI(jstring, nativeRate)(JNIEnv *env, jclass)
{
    std::lock_guard<std::mutex> lk(g_mu);
    return env->NewStringUTF(g_session ? g_session->RateText().c_str() : "");
}
