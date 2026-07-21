#include "http_parser.h"

#include <ctype.h>
#include <stdlib.h>

namespace legacyalpha {
namespace {

bool isTokenChar(unsigned char c) {
    return c > 0x20 && c < 0x7f && c != '(' && c != ')' && c != '<' &&
           c != '>' && c != '@' && c != ',' && c != ';' && c != ':' &&
           c != '\\' && c != '\"' && c != '/' && c != '[' && c != ']' &&
           c != '?' && c != '=' && c != '{' && c != '}';
}

std::string trim(const std::string &value) {
    size_t first = 0;
    size_t last = value.size();
    while (first < last && (value[first] == ' ' || value[first] == '\t')) ++first;
    while (last > first && (value[last - 1] == ' ' || value[last - 1] == '\t')) --last;
    return value.substr(first, last - first);
}

std::string lower(const std::string &value) {
    std::string result(value);
    for (size_t i = 0; i < result.size(); ++i) {
        result[i] = static_cast<char>(tolower(static_cast<unsigned char>(result[i])));
    }
    return result;
}

bool validPath(const std::string &path) {
    if (path.empty() || path[0] != '/' || path.find('\0') != std::string::npos) return false;
    if (path.find("..") != std::string::npos || path.find('\\') != std::string::npos) return false;
    for (size_t i = 0; i < path.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(path[i]);
        if (c < 0x20 || c == 0x7f) return false;
        if (c == '%') {
            if (i + 2 >= path.size() || !isxdigit(path[i + 1]) || !isxdigit(path[i + 2])) return false;
            return false;  // No endpoint requires percent-decoding.
        }
    }
    return path.find('?') == std::string::npos;
}

class JsonCursor {
public:
    explicit JsonCursor(const std::string &input) : input_(input), pos_(0) {}
    void ws() { while (pos_ < input_.size() && isspace(static_cast<unsigned char>(input_[pos_]))) ++pos_; }
    bool take(char c) { ws(); if (pos_ >= input_.size() || input_[pos_] != c) return false; ++pos_; return true; }
    bool done() { ws(); return pos_ == input_.size(); }
    bool stringValue(std::string *out) {
        ws();
        if (pos_ >= input_.size() || input_[pos_++] != '\"') return false;
        out->clear();
        while (pos_ < input_.size()) {
            unsigned char c = static_cast<unsigned char>(input_[pos_++]);
            if (c == '\"') return true;
            if (c < 0x20) return false;
            if (c == '\\') {
                if (pos_ >= input_.size()) return false;
                char e = input_[pos_++];
                if (e == '\"' || e == '\\' || e == '/') out->push_back(e);
                else if (e == 'b') out->push_back('\b');
                else if (e == 'f') out->push_back('\f');
                else if (e == 'n') out->push_back('\n');
                else if (e == 'r') out->push_back('\r');
                else if (e == 't') out->push_back('\t');
                else return false;
            } else out->push_back(static_cast<char>(c));
            if (out->size() > 128) return false;
        }
        return false;
    }
    bool integerValue(int *out) {
        ws();
        size_t start = pos_;
        if (pos_ < input_.size() && input_[pos_] == '-') ++pos_;
        if (pos_ >= input_.size() || !isdigit(static_cast<unsigned char>(input_[pos_]))) return false;
        while (pos_ < input_.size() && isdigit(static_cast<unsigned char>(input_[pos_]))) ++pos_;
        if (pos_ - start > 10) return false;
        char *end = NULL;
        long value = strtol(input_.substr(start, pos_ - start).c_str(), &end, 10);
        if (end == NULL || *end != '\0' || value < -2147483647L || value > 2147483647L) return false;
        *out = static_cast<int>(value);
        return true;
    }
    bool boolValue(bool *out) {
        ws();
        if (input_.compare(pos_, 4, "true") == 0) { pos_ += 4; *out = true; return true; }
        if (input_.compare(pos_, 5, "false") == 0) { pos_ += 5; *out = false; return true; }
        return false;
    }
private:
    const std::string &input_;
    size_t pos_;
};

}  // namespace

ParseResult parseHttpRequest(const std::string &data, const NativeConfig &config,
                             HttpRequest *request, size_t *consumed) {
    if (data.find('\0') != std::string::npos) return PARSE_BAD_REQUEST;
    size_t lineEnd = data.find("\r\n");
    if (lineEnd == std::string::npos) return data.size() > config.maxRequestLine ? PARSE_TOO_LARGE : PARSE_INCOMPLETE;
    if (lineEnd > config.maxRequestLine) return PARSE_TOO_LARGE;
    size_t firstSpace = data.find(' ');
    size_t secondSpace = firstSpace == std::string::npos ? std::string::npos : data.find(' ', firstSpace + 1);
    if (firstSpace == std::string::npos || secondSpace == std::string::npos || secondSpace >= lineEnd) return PARSE_BAD_REQUEST;
    request->method = data.substr(0, firstSpace);
    request->path = data.substr(firstSpace + 1, secondSpace - firstSpace - 1);
    std::string version = data.substr(secondSpace + 1, lineEnd - secondSpace - 1);
    if ((request->method != "GET" && request->method != "POST") || !validPath(request->path)) return PARSE_BAD_REQUEST;
    if (version != "HTTP/1.0" && version != "HTTP/1.1") return PARSE_UNSUPPORTED;
    for (size_t i = 0; i < request->method.size(); ++i) if (!isTokenChar(request->method[i])) return PARSE_BAD_REQUEST;

    size_t headerEnd = data.find("\r\n\r\n", lineEnd + 2);
    if (headerEnd == std::string::npos) return data.size() > config.maxHeaders ? PARSE_TOO_LARGE : PARSE_INCOMPLETE;
    if (headerEnd + 4 > config.maxHeaders) return PARSE_TOO_LARGE;
    request->pin.clear(); request->contentType.clear(); request->body.clear();
    bool haveLength = false;
    size_t contentLength = 0;
    size_t at = lineEnd + 2;
    while (at < headerEnd) {
        size_t end = data.find("\r\n", at);
        if (end == std::string::npos || end > headerEnd) return PARSE_BAD_REQUEST;
        size_t colon = data.find(':', at);
        if (colon == std::string::npos || colon >= end || colon == at) return PARSE_BAD_REQUEST;
        std::string name = data.substr(at, colon - at);
        for (size_t i = 0; i < name.size(); ++i) if (!isTokenChar(name[i])) return PARSE_BAD_REQUEST;
        std::string value = trim(data.substr(colon + 1, end - colon - 1));
        std::string lname = lower(name);
        if (lname == "content-length") {
            if (value.empty() || value[0] == '-') return PARSE_BAD_REQUEST;
            char *tail = NULL;
            unsigned long parsed = strtoul(value.c_str(), &tail, 10);
            if (tail == NULL || *tail != '\0') return PARSE_BAD_REQUEST;
            if (haveLength && parsed != contentLength) return PARSE_BAD_REQUEST;
            contentLength = static_cast<size_t>(parsed); haveLength = true;
            if (contentLength > config.maxBody) return PARSE_TOO_LARGE;
        } else if (lname == "transfer-encoding") {
            if (!value.empty() && lower(value) != "identity") return PARSE_UNSUPPORTED;
        } else if (lname == "x-legacyalpha-pin") {
            if (!request->pin.empty() && request->pin != value) return PARSE_BAD_REQUEST;
            request->pin = value;
        } else if (lname == "content-type") request->contentType = lower(value);
        at = end + 2;
    }
    if (request->method == "POST" && !haveLength) return PARSE_BAD_REQUEST;
    size_t total = headerEnd + 4 + contentLength;
    if (data.size() < total) return PARSE_INCOMPLETE;
    request->body.assign(data, headerEnd + 4, contentLength);
    *consumed = total;
    return PARSE_OK;
}

bool validRequestId(const std::string &requestId) {
    if (requestId.empty() || requestId.size() > 64) return false;
    for (size_t i = 0; i < requestId.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(requestId[i]);
        if (!isalnum(c) && c != '-' && c != '_' && c != '.' && c != ':') return false;
    }
    return true;
}

bool parseCaptureJson(const std::string &body, CaptureRequest *request) {
    JsonCursor c(body); bool id = false, delay = false, autofocus = false;
    if (!c.take('{')) return false;
    if (c.take('}')) return false;
    for (;;) {
        std::string key;
        if (!c.stringValue(&key) || !c.take(':')) return false;
        if (key == "request_id") { if (id || !c.stringValue(&request->requestId)) return false; id = true; }
        else if (key == "delay_ms") { if (delay || !c.integerValue(&request->delayMs)) return false; delay = true; }
        else if (key == "autofocus") { if (autofocus || !c.boolValue(&request->autofocus)) return false; autofocus = true; }
        else return false;
        if (c.take('}')) break;
        if (!c.take(',')) return false;
    }
    return c.done() && id && delay && autofocus && validRequestId(request->requestId);
}

bool parseFocusJson(const std::string &body, FocusRequest *request) {
    JsonCursor c(body); std::string key;
    return c.take('{') && c.stringValue(&key) && key == "pressed" && c.take(':') &&
           c.boolValue(&request->pressed) && c.take('}') && c.done();
}

}  // namespace legacyalpha
