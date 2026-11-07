#include <jni.h>
#include <android/log.h>
#include <memory>
#include <mutex>
#include <string>

#include "VoiceSession.h"

#define LOG_TAG "ravava"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {
    std::mutex g_mu;
    std::unique_ptr<VoiceSession> g_session;
    JavaVM *g_vm = nullptr;
}

extern "C" {

// ---------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL
Java_com_ravava_demo_VoiceService_nativeInit(JNIEnv *env, jclass /*clazz*/, jobject context)
{
    std::lock_guard<std::mutex> lk(g_mu);
    if (!g_vm) {
        env->GetJavaVM(&g_vm);
    }
    // Global ref so the context stays valid
    jobject globalCtx = env->NewGlobalRef(context);
    bool ok = VoiceSession::InitAndroid(g_vm, globalCtx);
    // Note: we keep the global ref for the lifetime of the process
    return ok ? JNI_TRUE : JNI_FALSE;
}

// ---------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL
Java_com_ravava_demo_VoiceService_nativeStart(JNIEnv *env, jclass /*clazz*/,
                                              jboolean udpMode, jint localPort,
                                              jstring remoteIp, jint remotePort)
{
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_session) {
        g_session->Stop();
        g_session.reset();
    }

    const char *ip = env->GetStringUTFChars(remoteIp, nullptr);
    std::string ipStr = ip ? ip : "127.0.0.1";
    env->ReleaseStringUTFChars(remoteIp, ip);

    g_session = std::make_unique<VoiceSession>();
    bool ok = g_session->Start(udpMode == JNI_TRUE, localPort, ipStr, remotePort);
    if (!ok) {
        g_session.reset();
    }
    return ok ? JNI_TRUE : JNI_FALSE;
}

// ---------------------------------------------------------------------------
JNIEXPORT void JNICALL
Java_com_ravava_demo_VoiceService_nativeStop(JNIEnv * /*env*/, jclass /*clazz*/)
{
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_session) {
        g_session->Stop();
        g_session.reset();
    }
}

// ---------------------------------------------------------------------------
JNIEXPORT jint JNICALL
Java_com_ravava_demo_VoiceService_nativeMicLevel(JNIEnv * /*env*/, jclass /*clazz*/)
{
    std::lock_guard<std::mutex> lk(g_mu);
    return g_session ? g_session->MicLevel() : 0;
}

JNIEXPORT jint JNICALL
Java_com_ravava_demo_VoiceService_nativeSpkLevel(JNIEnv * /*env*/, jclass /*clazz*/)
{
    std::lock_guard<std::mutex> lk(g_mu);
    return g_session ? g_session->SpkLevel() : 0;
}

JNIEXPORT jstring JNICALL
Java_com_ravava_demo_VoiceService_nativeStats(JNIEnv *env, jclass /*clazz*/)
{
    std::lock_guard<std::mutex> lk(g_mu);
    std::string s = g_session ? g_session->StatsText() : "Idle";
    return env->NewStringUTF(s.c_str());
}

} // extern "C"
