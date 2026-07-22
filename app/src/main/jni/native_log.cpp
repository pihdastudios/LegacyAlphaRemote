#include "native_log.h"

#include <stdio.h>
#include <time.h>
#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace legacyalpha {
namespace {
std::string g_path;
}

void nativeLogSetPath(const std::string& path) {
    g_path = path;
}

void nativeLog(const char* level, const std::string& message) {
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "LegacyAlphaRemote", "%s: %s", level, message.c_str());
#endif
    if (g_path.empty())
        return;
    FILE* file = fopen(g_path.c_str(), "a");
    if (file == NULL)
        return;
    time_t now = time(NULL);
    struct tm local;
    localtime_r(&now, &local);
    char stamp[32];
    strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &local);
    fprintf(file, "%s [%s] %s\n", stamp, level, message.c_str());
    fclose(file);
}

} // namespace legacyalpha
