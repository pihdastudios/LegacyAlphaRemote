#ifndef LEGACY_ALPHA_REQUEST_DEDUPLICATOR_H
#define LEGACY_ALPHA_REQUEST_DEDUPLICATOR_H

#include <stdint.h>
#include <string>
#include <vector>

namespace legacyalpha {

class RequestDeduplicator {
public:
    RequestDeduplicator(size_t maximum, int64_t retentionMs);
    bool find(const std::string &requestId, int64_t nowMs, int *status,
              std::string *body);
    void remember(const std::string &requestId, int status,
                  const std::string &body, int64_t nowMs);
    void clear();
private:
    struct Entry {
        std::string requestId;
        int status;
        std::string body;
        int64_t expiresMs;
    };
    void prune(int64_t nowMs);
    size_t maximum_;
    int64_t retentionMs_;
    std::vector<Entry> entries_;
};

}  // namespace legacyalpha

#endif
