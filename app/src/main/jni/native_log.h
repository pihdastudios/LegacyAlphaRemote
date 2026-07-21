#ifndef LEGACY_ALPHA_NATIVE_LOG_H
#define LEGACY_ALPHA_NATIVE_LOG_H

#include <string>

namespace legacyalpha {

void nativeLogSetPath(const std::string &path);
void nativeLog(const char *level, const std::string &message);

}  // namespace legacyalpha

#endif
