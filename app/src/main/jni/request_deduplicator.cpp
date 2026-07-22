#include "request_deduplicator.h"

namespace legacyalpha {

RequestDeduplicator::RequestDeduplicator(size_t maximum, int64_t retentionMs)
    : maximum_(maximum), retentionMs_(retentionMs) {}

void RequestDeduplicator::prune(int64_t nowMs) {
    for (size_t i = 0; i < entries_.size();) {
        if (entries_[i].expiresMs <= nowMs)
            entries_.erase(entries_.begin() + i);
        else
            ++i;
    }
}

bool RequestDeduplicator::find(const std::string& requestId, int64_t nowMs, int* status,
                               std::string* body) {
    prune(nowMs);
    for (size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].requestId == requestId) {
            *status = entries_[i].status;
            *body = entries_[i].body;
            return true;
        }
    }
    return false;
}

void RequestDeduplicator::remember(const std::string& requestId, int status,
                                   const std::string& body, int64_t nowMs) {
    prune(nowMs);
    if (entries_.size() >= maximum_)
        entries_.erase(entries_.begin());
    Entry entry;
    entry.requestId = requestId;
    entry.status = status;
    entry.body = body;
    entry.expiresMs = nowMs + retentionMs_;
    entries_.push_back(entry);
}

void RequestDeduplicator::clear() {
    entries_.clear();
}

} // namespace legacyalpha
