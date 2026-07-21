#include "native_runtime.h"
#include "embedded_web.h"
#include "http_response.h"
#include "json_writer.h"
#include "native_log.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

namespace legacyalpha {
namespace {
NativeConfig configForPort(int port) {
    NativeConfig config;
    if (port > 0 && port <= 65535) config.port = port;
    return config;
}

class Lock {
public:
    explicit Lock(pthread_mutex_t *mutex) : mutex_(mutex) { pthread_mutex_lock(mutex_); }
    ~Lock() { pthread_mutex_unlock(mutex_); }
private:
    pthread_mutex_t *mutex_;
};

bool contentTypeIsJson(const std::string &value) {
    return value == "application/json" || value.find("application/json;") == 0;
}
}

NativeRuntime::NativeRuntime(JavaDispatcher &dispatcher, int port)
    : dispatcher_(dispatcher), config_(configForPort(port)), clock_(), state_(config_, clock_, *this),
      server_(config_, *this), deduplicator_(64, config_.dedupeRetentionMs),
      mutationLimiter_(6.0, 1.0, 16), statusLimiter_(20.0, 4.0, 16),
      started_(false), address_("192.168.122.1") {
    pthread_mutex_init(&mutex_, NULL);
}

NativeRuntime::~NativeRuntime() { stop(); pthread_mutex_destroy(&mutex_); }

bool NativeRuntime::generatePin() {
    unsigned int randomValue = 0;
    int fd = open("/dev/urandom", O_RDONLY);
    bool strong = fd >= 0 && read(fd, &randomValue, sizeof(randomValue)) == sizeof(randomValue);
    if (fd >= 0) close(fd);
    if (!strong) {
        randomValue = static_cast<unsigned int>(clock_.nowMs()) ^
                      static_cast<unsigned int>(getpid() * 2654435761UL) ^
                      static_cast<unsigned int>(time(NULL));
        nativeLog("WARN", "strong PIN randomness unavailable; fallback used");
    }
    char value[7]; snprintf(value, sizeof(value), "%06u", randomValue % 1000000U);
    pin_ = value; return strong;
}

bool NativeRuntime::start() {
    Lock lock(&mutex_);
    if (started_) return true;
    generatePin(); serverError_.clear(); wifiError_.clear(); deduplicator_.clear();
    mutationLimiter_.clear(); statusLimiter_.clear(); state_.start();
    if (!server_.start()) { state_.stop(); pin_.clear(); return false; }
    started_ = true; nativeLog("INFO", "native runtime version 0.1.0 started");
    updateScreen(true); return true;
}

void NativeRuntime::stop() {
    {
        Lock lock(&mutex_);
        if (!started_) return;
        started_ = false; state_.stop(); pin_.clear();
    }
    server_.stop(); nativeLog("INFO", "native runtime stopped");
}

void NativeRuntime::setCameraReady(bool ready) {
    Lock lock(&mutex_); state_.setCameraReady(ready); updateScreen(true); server_.wake();
}

void NativeRuntime::setWifiState(bool ready, const std::string &ssid,
                                 const std::string &password,
                                 const std::string &address, int stationCount,
                                 const std::string &error) {
    Lock lock(&mutex_);
    ssid_ = ssid; password_ = password;
    if (!address.empty()) address_ = address;
    wifiError_ = error == "stopped" ? "" : error;
    state_.setClientCount(stationCount); state_.setWifiReady(ready);
    updateScreen(true); server_.wake();
}

void NativeRuntime::reportCommandResult(int64_t commandId, int resultCode,
                                        const std::string &message) {
    Lock lock(&mutex_); state_.reportCommandResult(commandId, resultCode, message); updateScreen(true); server_.wake();
}

bool NativeRuntime::physicalFocus(bool pressed) {
    Lock lock(&mutex_); std::string error; bool ok = state_.setFocus(pressed, &error); updateScreen(true); server_.wake(); return ok;
}

bool NativeRuntime::physicalShutter(bool pressed) {
    Lock lock(&mutex_); std::string error; bool ok = state_.physicalShutter(pressed, &error); updateScreen(true); server_.wake(); return ok;
}

bool NativeRuntime::dispatch(const NativeCommand &command) {
    return started_ && dispatcher_.dispatchToJava(command);
}

bool NativeRuntime::serverReady() {
    Lock lock(&mutex_); return started_ && state_.cameraReady() && state_.wifiReady();
}

std::string NativeRuntime::listenAddress() { Lock lock(&mutex_); return address_; }

bool NativeRuntime::authenticate(const std::string &candidate) const {
    unsigned int difference = static_cast<unsigned int>(candidate.size() ^ pin_.size());
    for (size_t i = 0; i < 6; ++i) {
        unsigned char a = i < candidate.size() ? candidate[i] : 0;
        unsigned char b = i < pin_.size() ? pin_[i] : 0;
        difference |= a ^ b;
    }
    return difference == 0;
}

std::string NativeRuntime::statusJson() const {
    const std::string error = !state_.lastError().empty() ? state_.lastError() :
                              !wifiError_.empty() ? wifiError_ : serverError_;
    char numbers[192];
    snprintf(numbers, sizeof(numbers),
             ",\"wifi_ready\":%s,\"camera_ready\":%s,\"client_count\":%d,\"countdown_remaining_ms\":%d",
             state_.wifiReady() ? "true" : "false", state_.cameraReady() ? "true" : "false",
             state_.clientCount(), state_.countdownRemainingMs());
    return std::string("{\"ok\":true,\"version\":\"0.1.0\",\"state\":") +
           jsonString(stateName(state_.state())) + numbers +
           ",\"last_request_id\":" + jsonString(state_.lastRequestId()) +
           ",\"last_error\":" + jsonString(error) + "}";
}

std::string NativeRuntime::screenText() const {
    const std::string error = !state_.lastError().empty() ? state_.lastError() :
                              !wifiError_.empty() ? wifiError_ : serverError_;
    char values[512];
    snprintf(values, sizeof(values),
             "Legacy Alpha Remote\n\nNetwork: %s\nPassword: %s\nAddress: %s:%d\nURL: http://%s:%d/\nPIN: %s\n\nStatus: %s\nPhone connected: %s",
             ssid_.empty() ? "Starting..." : ssid_.c_str(),
             password_.empty() ? "not exposed" : password_.c_str(),
             address_.c_str(), config_.port, address_.c_str(), config_.port,
             pin_.c_str(), stateName(state_.state()), state_.clientCount() > 0 ? "Yes" : "No");
    std::string result(values);
    if (state_.countdownRemainingMs() > 0) {
        char countdown[64]; snprintf(countdown, sizeof(countdown), "\nCountdown: %.1f s", state_.countdownRemainingMs() / 1000.0); result += countdown;
    }
    if (!error.empty()) result += "\n\nError: " + error;
    return result;
}

void NativeRuntime::updateScreen(bool force) {
    if (!started_) return;
    std::string current = screenText();
    if (!force && current == previousScreen_) return;
    previousScreen_ = current;
    NativeCommand command; command.id = 0; command.type = COMMAND_UPDATE_SCREEN; command.text = current;
    dispatcher_.dispatchToJava(command);
}

std::string NativeRuntime::handleRequest(const HttpRequest &request,
                                         const std::string &remoteAddress) {
    Lock lock(&mutex_);
    nativeLog("INFO", std::string("HTTP ") + request.method + " " + request.path);
    const int64_t now = clock_.nowMs();
    if (request.method == "GET") {
        if (!statusLimiter_.allow(remoteAddress, now)) { nativeLog("WARN", "status rate limit"); return makeJsonError(429, "rate limited"); }
        if (request.path == "/") return makeHttpResponse(200, "text/html; charset=utf-8", kIndexHtml);
        if (request.path == "/style.css") return makeHttpResponse(200, "text/css; charset=utf-8", kStyleCss);
        if (request.path == "/app.js") return makeHttpResponse(200, "application/javascript; charset=utf-8", kAppJs);
        if (request.path == "/api/v1/status") return makeHttpResponse(200, "application/json; charset=utf-8", statusJson());
        return makeJsonError(404, "not found");
    }
    if (request.method != "POST") return makeJsonError(405, "method not allowed");
    if (!mutationLimiter_.allow(remoteAddress, now)) { nativeLog("WARN", "mutation rate limit"); return makeJsonError(429, "rate limited"); }
    if (!authenticate(request.pin)) { nativeLog("WARN", "authentication rejected"); return makeJsonError(401, "authentication required"); }
    if (!contentTypeIsJson(request.contentType)) return makeJsonError(415, "application/json required");

    if (request.path == "/api/v1/capture") {
        CaptureRequest capture;
        if (!parseCaptureJson(request.body, &capture)) return makeJsonError(400, "invalid capture request");
        int oldStatus; std::string oldBody;
        if (deduplicator_.find(capture.requestId, now, &oldStatus, &oldBody)) {
            nativeLog("INFO", std::string("deduplicated request ") + capture.requestId);
            return makeHttpResponse(oldStatus, "application/json; charset=utf-8", oldBody);
        }
        std::string error; int status = 202; std::string body;
        if (capture.delayMs < 0 || capture.delayMs > config_.maxDelayMs) {
            status = 400; body = "{\"ok\":false,\"error\":\"delay_ms is out of range\"}";
        } else if (!state_.requestCapture(capture.requestId, capture.delayMs, capture.autofocus, &error)) {
            status = 409; body = std::string("{\"ok\":false,\"accepted\":false,\"request_id\":") + jsonString(capture.requestId) + ",\"error\":" + jsonString(error) + "}";
        } else {
            body = std::string("{\"ok\":true,\"accepted\":true,\"request_id\":") + jsonString(capture.requestId) + ",\"state\":" + jsonString(stateName(state_.state())) + "}";
        }
        deduplicator_.remember(capture.requestId, status, body, now); updateScreen(true); server_.wake();
        return makeHttpResponse(status, "application/json; charset=utf-8", body);
    }
    if (request.path == "/api/v1/cancel") {
        if (request.body != "{}" && !request.body.empty()) return makeJsonError(400, "invalid cancel request");
        std::string error;
        if (!state_.cancelCountdown(&error)) return makeJsonError(409, error);
        updateScreen(true); return makeHttpResponse(200, "application/json; charset=utf-8", "{\"ok\":true,\"cancelled\":true}");
    }
    if (request.path == "/api/v1/focus") {
        FocusRequest focus; std::string error;
        if (!parseFocusJson(request.body, &focus)) return makeJsonError(400, "invalid focus request");
        if (!state_.setFocus(focus.pressed, &error)) return makeJsonError(409, error);
        updateScreen(true); return makeHttpResponse(200, "application/json; charset=utf-8", "{\"ok\":true}");
    }
    return makeJsonError(404, "not found");
}

void NativeRuntime::onServerTick() {
    Lock lock(&mutex_); if (!started_) return; state_.tick(); updateScreen(false);
}

void NativeRuntime::onServerError(const std::string &error) {
    Lock lock(&mutex_); serverError_ = error; updateScreen(true);
}

}  // namespace legacyalpha
