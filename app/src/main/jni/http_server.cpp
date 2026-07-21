#include "http_server.h"
#include "http_response.h"
#include "monotonic_clock.h"
#include "native_log.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

namespace legacyalpha {
namespace {
int nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return flags < 0 ? -1 : fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
std::string portText(int port) {
    char value[16]; snprintf(value, sizeof(value), "%d", port); return value;
}
}

HttpServer::HttpServer(const NativeConfig &config, HttpRequestHandler &handler)
    : config_(config), handler_(handler), threadCreated_(false), stopping_(false),
      running_(false), listener_(-1), nextListenAttemptMs_(0) {
    wakePipe_[0] = wakePipe_[1] = -1;
}

HttpServer::~HttpServer() { stop(); }

bool HttpServer::start() {
    if (threadCreated_) return true;
    if (pipe(wakePipe_) != 0) return false;
    nonblocking(wakePipe_[0]); nonblocking(wakePipe_[1]);
    stopping_ = false;
    if (pthread_create(&thread_, NULL, threadEntry, this) != 0) {
        close(wakePipe_[0]); close(wakePipe_[1]); wakePipe_[0] = wakePipe_[1] = -1;
        return false;
    }
    threadCreated_ = true;
    return true;
}

void HttpServer::stop() {
    if (!threadCreated_) return;
    stopping_ = true; wake();
    pthread_join(thread_, NULL);
    threadCreated_ = false;
    if (wakePipe_[0] >= 0) close(wakePipe_[0]);
    if (wakePipe_[1] >= 0) close(wakePipe_[1]);
    wakePipe_[0] = wakePipe_[1] = -1;
}

void HttpServer::wake() {
    if (wakePipe_[1] < 0) return;
    const char byte = 'w';
    ssize_t ignored = write(wakePipe_[1], &byte, 1); (void) ignored;
}

void *HttpServer::threadEntry(void *value) {
    static_cast<HttpServer *>(value)->run(); return NULL;
}

bool HttpServer::openListener(const std::string &address) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET; addr.sin_port = htons(config_.port);
    if (address.empty() || address == "0.0.0.0") {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else if (inet_aton(address.c_str(), &addr.sin_addr) == 0) {
        // Some Sony firmware reports the P2P interface identifier instead of
        // its IPv4 address. Bind all local interfaces and keep the Java-side
        // validated default URL for that case.
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        nativeLog("WARN", std::string("invalid bind address '") + address + "'; using wildcard");
    }
    if (bind(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) != 0) {
        nativeLog("ERROR", std::string("bind failed on port ") + portText(config_.port) + ": " + strerror(errno));
        close(fd); return false;
    }
    if (listen(fd, static_cast<int>(config_.maxClients)) != 0 || nonblocking(fd) != 0) {
        nativeLog("ERROR", std::string("listen setup failed on port ") + portText(config_.port) + ": " + strerror(errno));
        close(fd); return false;
    }
    listener_ = fd;
    nativeLog("INFO", std::string("server listening on ") + address);
    return true;
}

void HttpServer::closeListener() {
    if (listener_ >= 0) { close(listener_); listener_ = -1; nativeLog("INFO", "server listener stopped"); }
}

void HttpServer::closeClient(size_t index) {
    if (clients_[index].fd >= 0) close(clients_[index].fd);
    clients_.erase(clients_.begin() + index);
}

void HttpServer::acceptClient(int64_t nowMs) {
    struct sockaddr_in address; socklen_t length = sizeof(address);
    int fd = accept(listener_, reinterpret_cast<struct sockaddr *>(&address), &length);
    if (fd < 0) return;
    if (clients_.size() >= config_.maxClients || nonblocking(fd) != 0) { close(fd); return; }
    struct timeval timeout; timeout.tv_sec = config_.idleTimeoutMs / 1000; timeout.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    Client client; client.fd = fd; client.lastActivityMs = nowMs;
    char ip[INET_ADDRSTRLEN];
    client.remoteAddress = inet_ntop(AF_INET, &address.sin_addr, ip, sizeof(ip)) == NULL ? "unknown" : ip;
    clients_.push_back(client);
}

void HttpServer::readClient(size_t index, int64_t nowMs) {
    char buffer[2048];
    ssize_t count = recv(clients_[index].fd, buffer, sizeof(buffer), 0);
    if (count == 0) { closeClient(index); return; }
    if (count < 0) { if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) closeClient(index); return; }
    clients_[index].lastActivityMs = nowMs;
    clients_[index].input.append(buffer, static_cast<size_t>(count));
    if (clients_[index].input.size() > config_.maxHeaders + config_.maxBody) {
        clients_[index].output = makeJsonError(413, "request too large"); return;
    }
    HttpRequest request; size_t consumed = 0;
    ParseResult parsed = parseHttpRequest(clients_[index].input, config_, &request, &consumed);
    if (parsed == PARSE_INCOMPLETE) return;
    if (parsed != PARSE_OK) {
        clients_[index].output = makeJsonError(parsed == PARSE_TOO_LARGE ? 413 :
                                               parsed == PARSE_UNSUPPORTED ? 415 : 400,
                                               "invalid request");
        return;
    }
    clients_[index].output = handler_.handleRequest(request, clients_[index].remoteAddress);
}

void HttpServer::writeClient(size_t index, int64_t nowMs) {
    Client &client = clients_[index];
    if (client.written >= client.output.size()) { closeClient(index); return; }
    const char *data = client.output.data() + client.written;
    size_t left = client.output.size() - client.written;
#ifdef MSG_NOSIGNAL
    ssize_t count = send(client.fd, data, left, MSG_NOSIGNAL);
#else
    ssize_t count = send(client.fd, data, left, 0);
#endif
    if (count > 0) { client.written += static_cast<size_t>(count); client.lastActivityMs = nowMs; }
    else if (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) { closeClient(index); return; }
    if (index < clients_.size() && clients_[index].written == clients_[index].output.size()) closeClient(index);
}

void HttpServer::run() {
    signal(SIGPIPE, SIG_IGN); running_ = true; nativeLog("INFO", "server thread started");
    MonotonicClock clock;
    while (!stopping_) {
        const int64_t now = clock.nowMs();
        handler_.onServerTick();
        if (!handler_.serverReady()) closeListener();
        else if (listener_ < 0 && now >= nextListenAttemptMs_) {
            if (!openListener(handler_.listenAddress())) {
                nextListenAttemptMs_ = now + 1000;
                handler_.onServerError(std::string("HTTP listener could not bind on port ") + portText(config_.port));
            }
        }
        std::vector<struct pollfd> descriptors;
        struct pollfd wakeFd; wakeFd.fd = wakePipe_[0]; wakeFd.events = POLLIN; wakeFd.revents = 0; descriptors.push_back(wakeFd);
        if (listener_ >= 0) { struct pollfd p; p.fd = listener_; p.events = POLLIN; p.revents = 0; descriptors.push_back(p); }
        for (size_t i = 0; i < clients_.size(); ++i) {
            struct pollfd p; p.fd = clients_[i].fd; p.events = clients_[i].output.empty() ? POLLIN : POLLOUT; p.revents = 0; descriptors.push_back(p);
        }
        int result = poll(&descriptors[0], descriptors.size(), 250);
        if (result < 0 && errno != EINTR) handler_.onServerError("HTTP poll failed");
        size_t offset = 0;
        if (descriptors[offset++].revents & POLLIN) { char drain[64]; while (read(wakePipe_[0], drain, sizeof(drain)) > 0) {} }
        if (listener_ >= 0 && offset < descriptors.size()) {
            if (descriptors[offset].revents & POLLIN) acceptClient(clock.nowMs());
            ++offset;
        }
        for (size_t remaining = clients_.size(); remaining > 0; --remaining) {
            size_t i = remaining - 1;
            short events = offset + i < descriptors.size() ? descriptors[offset + i].revents : 0;
            if (events & (POLLERR | POLLHUP | POLLNVAL)) closeClient(i);
            else if (events & POLLIN) readClient(i, clock.nowMs());
            else if (events & POLLOUT) writeClient(i, clock.nowMs());
        }
        const int64_t after = clock.nowMs();
        for (size_t i = 0; i < clients_.size();) {
            if (after - clients_[i].lastActivityMs > config_.idleTimeoutMs) closeClient(i); else ++i;
        }
    }
    closeListener();
    while (!clients_.empty()) closeClient(0);
    running_ = false; nativeLog("INFO", "server thread stopped");
}

}  // namespace legacyalpha
