#ifndef LEGACY_ALPHA_NATIVE_RUNTIME_H
#define LEGACY_ALPHA_NATIVE_RUNTIME_H

#include "http_server.h"
#include "rate_limiter.h"
#include "remote_state_machine.h"
#include "request_deduplicator.h"

#include <pthread.h>
#include <vector>

namespace legacyalpha {

class JavaDispatcher {
  public:
    virtual ~JavaDispatcher() {}
    virtual bool dispatchToJava(const NativeCommand& command) = 0;
};

class NativeRuntime : public CommandSink, public HttpRequestHandler {
  public:
    NativeRuntime(JavaDispatcher& dispatcher, int port);
    ~NativeRuntime();
    bool start();
    void stop();
    void setCameraReady(bool ready);
    void setWifiState(bool ready, const std::string& ssid, const std::string& password,
                      const std::string& address, int stationCount, const std::string& error);
    void reportCommandResult(int64_t commandId, int resultCode, const std::string& message);
    bool physicalFocus(bool pressed);
    bool physicalShutter(bool pressed);
    void setPreview(const unsigned char* data, size_t size, int width, int height,
                    const std::string& source, const std::string& error);

    virtual bool dispatch(const NativeCommand& command);
    virtual bool serverReady();
    virtual std::string listenAddress();
    virtual std::string handleRequest(const HttpRequest& request, const std::string& remoteAddress);
    virtual void onServerTick();
    virtual void onServerError(const std::string& error);

  private:
    bool authenticate(const std::string& candidate) const;
    std::string statusJson() const;
    std::string screenText() const;
    void updateScreen(bool force);
    bool generatePin();

    JavaDispatcher& dispatcher_;
    NativeConfig config_;
    MonotonicClock clock_;
    RemoteStateMachine state_;
    HttpServer server_;
    RequestDeduplicator deduplicator_;
    RateLimiter mutationLimiter_;
    RateLimiter statusLimiter_;
    pthread_mutex_t mutex_;
    bool started_;
    std::string pin_;
    std::string ssid_;
    std::string password_;
    std::string address_;
    std::string wifiError_;
    std::string serverError_;
    std::string previousScreen_;
    std::vector<unsigned char> previewJpeg_;
    int previewWidth_;
    int previewHeight_;
    int64_t previewVersion_;
    std::string previewSource_;
    std::string previewError_;
};

} // namespace legacyalpha

#endif
