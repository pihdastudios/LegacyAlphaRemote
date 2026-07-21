#ifndef LEGACY_ALPHA_HTTP_SERVER_H
#define LEGACY_ALPHA_HTTP_SERVER_H

#include "http_parser.h"

#include <pthread.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace legacyalpha {

class HttpRequestHandler {
public:
    virtual ~HttpRequestHandler() {}
    virtual bool serverReady() = 0;
    virtual std::string listenAddress() = 0;
    virtual std::string handleRequest(const HttpRequest &request,
                                      const std::string &remoteAddress) = 0;
    virtual void onServerTick() = 0;
    virtual void onServerError(const std::string &error) = 0;
};

class HttpServer {
public:
    HttpServer(const NativeConfig &config, HttpRequestHandler &handler);
    ~HttpServer();
    bool start();
    void stop();
    void wake();
    bool running() const { return running_; }
private:
    struct Client {
        int fd;
        std::string input;
        std::string output;
        size_t written;
        int64_t lastActivityMs;
        std::string remoteAddress;
        Client() : fd(-1), written(0), lastActivityMs(0) {}
    };
    static void *threadEntry(void *value);
    void run();
    bool openListener(const std::string &address);
    void closeListener();
    void closeClient(size_t index);
    void acceptClient(int64_t nowMs);
    void readClient(size_t index, int64_t nowMs);
    void writeClient(size_t index, int64_t nowMs);

    NativeConfig config_;
    HttpRequestHandler &handler_;
    pthread_t thread_;
    bool threadCreated_;
    volatile bool stopping_;
    volatile bool running_;
    int wakePipe_[2];
    int listener_;
    int64_t nextListenAttemptMs_;
    std::vector<Client> clients_;
};

}  // namespace legacyalpha

#endif
