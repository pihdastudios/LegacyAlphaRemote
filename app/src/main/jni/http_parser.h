#ifndef LEGACY_ALPHA_HTTP_PARSER_H
#define LEGACY_ALPHA_HTTP_PARSER_H

#include "remote_types.h"

#include <string>

namespace legacyalpha {

enum ParseResult {
    PARSE_INCOMPLETE,
    PARSE_OK,
    PARSE_BAD_REQUEST,
    PARSE_TOO_LARGE,
    PARSE_UNSUPPORTED
};

struct HttpRequest {
    std::string method;
    std::string path;
    std::string pin;
    std::string contentType;
    std::string body;
};

struct CaptureRequest {
    std::string requestId;
    int delayMs;
    bool autofocus;
    CaptureRequest() : delayMs(0), autofocus(true) {}
};

struct FocusRequest {
    bool pressed;
    FocusRequest() : pressed(false) {}
};

ParseResult parseHttpRequest(const std::string &data, const NativeConfig &config,
                             HttpRequest *request, size_t *consumed);
bool parseCaptureJson(const std::string &body, CaptureRequest *request);
bool parseFocusJson(const std::string &body, FocusRequest *request);
bool validRequestId(const std::string &requestId);

}  // namespace legacyalpha

#endif
