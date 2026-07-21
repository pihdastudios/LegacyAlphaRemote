#ifndef LEGACY_ALPHA_JSON_WRITER_H
#define LEGACY_ALPHA_JSON_WRITER_H

#include <string>

namespace legacyalpha {

std::string jsonEscape(const std::string &value);
std::string jsonString(const std::string &value);

}  // namespace legacyalpha

#endif
