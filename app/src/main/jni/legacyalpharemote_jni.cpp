#include "native_runtime.h"
#include "native_log.h"

#include <jni.h>

using namespace legacyalpha;

namespace {
JavaVM *g_vm = NULL;
jobject g_activity = NULL;
jmethodID g_dispatch = NULL;
NativeRuntime *g_runtime = NULL;

class AndroidDispatcher : public JavaDispatcher {
public:
    virtual bool dispatchToJava(const NativeCommand &command) {
        if (g_vm == NULL || g_activity == NULL || g_dispatch == NULL) return false;
        JNIEnv *env = NULL;
        bool attached = false;
        if (g_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
#ifdef __ANDROID__
            if (g_vm->AttachCurrentThread(&env, NULL) != JNI_OK) return false;
#else
            if (g_vm->AttachCurrentThread(reinterpret_cast<void **>(&env), NULL) != JNI_OK) return false;
#endif
            attached = true;
        }
        jstring text = env->NewStringUTF(command.text.c_str());
        env->CallVoidMethod(g_activity, g_dispatch, static_cast<jlong>(command.id),
                            static_cast<jint>(command.type), static_cast<jint>(command.argument), text);
        if (text != NULL) env->DeleteLocalRef(text);
        bool successful = !env->ExceptionCheck();
        if (attached) g_vm->DetachCurrentThread();
        return successful;
    }
};
AndroidDispatcher g_dispatcher;

void deleteActivity(JNIEnv *env) {
    if (g_activity != NULL) { env->DeleteGlobalRef(g_activity); g_activity = NULL; }
    g_dispatch = NULL;
}
}

extern "C" jint JNI_OnLoad(JavaVM *vm, void *) {
    g_vm = vm; return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT jstring JNICALL
Java_io_pihda_legacyalpharemote_NativeBridge_nativeVersion(JNIEnv *env, jclass) {
    return env->NewStringUTF("0.1.0");
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_pihda_legacyalpharemote_NativeBridge_nativeStart(JNIEnv *env, jclass,
                                                         jobject activity,
                                                         jstring logPath, jint port) {
    if (g_runtime != NULL) return JNI_TRUE;
    jclass clazz = env->GetObjectClass(activity);
    g_dispatch = env->GetMethodID(clazz, "dispatchNativeCommand", "(JIILjava/lang/String;)V");
    if (g_dispatch == NULL) return JNI_FALSE;
    g_activity = env->NewGlobalRef(activity);
    const char *path = logPath == NULL ? NULL : env->GetStringUTFChars(logPath, NULL);
    if (path != NULL) nativeLogSetPath(path);
    g_runtime = new NativeRuntime(g_dispatcher, static_cast<int>(port));
    bool started = g_runtime->start();
    if (path != NULL) env->ReleaseStringUTFChars(logPath, path);
    if (!started) { delete g_runtime; g_runtime = NULL; deleteActivity(env); }
    return started ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_io_pihda_legacyalpharemote_NativeBridge_nativeStop(JNIEnv *env, jclass) {
    if (g_runtime != NULL) { g_runtime->stop(); delete g_runtime; g_runtime = NULL; }
    deleteActivity(env);
}

extern "C" JNIEXPORT void JNICALL
Java_io_pihda_legacyalpharemote_NativeBridge_nativeSetCameraReady(JNIEnv *, jclass, jboolean ready) {
    if (g_runtime != NULL) g_runtime->setCameraReady(ready == JNI_TRUE);
}

extern "C" JNIEXPORT void JNICALL
Java_io_pihda_legacyalpharemote_NativeBridge_nativeSetWifiState(JNIEnv *env, jclass,
        jboolean ready, jstring ssid, jstring password, jstring address,
        jint stationCount, jstring error) {
    if (g_runtime == NULL) return;
    const char *s = ssid == NULL ? "" : env->GetStringUTFChars(ssid, NULL);
    const char *p = password == NULL ? "" : env->GetStringUTFChars(password, NULL);
    const char *a = address == NULL ? "" : env->GetStringUTFChars(address, NULL);
    const char *e = error == NULL ? "" : env->GetStringUTFChars(error, NULL);
    g_runtime->setWifiState(ready == JNI_TRUE, s, p, a, stationCount, e);
    if (ssid != NULL) env->ReleaseStringUTFChars(ssid, s);
    if (password != NULL) env->ReleaseStringUTFChars(password, p);
    if (address != NULL) env->ReleaseStringUTFChars(address, a);
    if (error != NULL) env->ReleaseStringUTFChars(error, e);
}

extern "C" JNIEXPORT void JNICALL
Java_io_pihda_legacyalpharemote_NativeBridge_nativeReportCommandResult(JNIEnv *env, jclass,
        jlong commandId, jint, jint resultCode, jstring message) {
    if (g_runtime == NULL) return;
    const char *m = message == NULL ? "" : env->GetStringUTFChars(message, NULL);
    g_runtime->reportCommandResult(commandId, resultCode, m);
    if (message != NULL) env->ReleaseStringUTFChars(message, m);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_pihda_legacyalpharemote_NativeBridge_nativePhysicalFocus(JNIEnv *, jclass, jboolean pressed) {
    return g_runtime != NULL && g_runtime->physicalFocus(pressed == JNI_TRUE) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_pihda_legacyalpharemote_NativeBridge_nativePhysicalShutter(JNIEnv *, jclass, jboolean pressed) {
    return g_runtime != NULL && g_runtime->physicalShutter(pressed == JNI_TRUE) ? JNI_TRUE : JNI_FALSE;
}
