#include "http_response.h"
#include "json_writer.h"

#include <stdio.h>

namespace legacyalpha {

static const char* reason(int status) {
    switch (status) {
        case 200:
            return "OK";
        case 202:
            return "Accepted";
        case 400:
            return "Bad Request";
        case 401:
            return "Unauthorized";
        case 404:
            return "Not Found";
        case 405:
            return "Method Not Allowed";
        case 409:
            return "Conflict";
        case 413:
            return "Payload Too Large";
        case 415:
            return "Unsupported Media Type";
        case 429:
            return "Too Many Requests";
        case 503:
            return "Service Unavailable";
        default:
            return "Internal Server Error";
    }
}

std::string makeHttpResponse(int status, const char* contentType, const std::string& body) {
    char header[384];
    snprintf(
        header, sizeof(header),
        "HTTP/1.0 %d %s\r\nContent-Type: %s\r\nContent-Length: %lu\r\n"
        "Cache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\n\r\n",
        status, reason(status), contentType, static_cast<unsigned long>(body.size()));
    return std::string(header) + body;
}

std::string makeJsonError(int status, const std::string& message) {
    return makeHttpResponse(status, "application/json; charset=utf-8",
                            std::string("{\"ok\":false,\"error\":") + jsonString(message) + "}");
}

} // namespace legacyalpha
