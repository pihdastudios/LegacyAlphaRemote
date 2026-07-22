#ifndef LEGACY_ALPHA_HTTP_RESPONSE_H
#define LEGACY_ALPHA_HTTP_RESPONSE_H

#include <string>

namespace legacyalpha {

std::string makeHttpResponse(int status, const char* contentType, const std::string& body);
std::string makeJsonError(int status, const std::string& message);

} // namespace legacyalpha

#endif
