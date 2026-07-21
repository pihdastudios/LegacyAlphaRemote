#include "rate_limiter.h"

namespace legacyalpha {

RateLimiter::RateLimiter(double capacity, double refillPerSecond, size_t maximumKeys)
    : capacity_(capacity), refillPerMs_(refillPerSecond / 1000.0),
      maximumKeys_(maximumKeys) {}

bool RateLimiter::allow(const std::string &key, int64_t nowMs) {
    Bucket *bucket = NULL;
    for (size_t i = 0; i < buckets_.size(); ++i) {
        if (buckets_[i].key == key) { bucket = &buckets_[i]; break; }
    }
    if (bucket == NULL) {
        if (buckets_.size() >= maximumKeys_) buckets_.erase(buckets_.begin());
        Bucket fresh; fresh.key = key; fresh.tokens = capacity_; fresh.lastMs = nowMs;
        buckets_.push_back(fresh); bucket = &buckets_.back();
    }
    if (nowMs > bucket->lastMs) {
        bucket->tokens += (nowMs - bucket->lastMs) * refillPerMs_;
        if (bucket->tokens > capacity_) bucket->tokens = capacity_;
        bucket->lastMs = nowMs;
    }
    if (bucket->tokens < 1.0) return false;
    bucket->tokens -= 1.0;
    return true;
}

void RateLimiter::clear() { buckets_.clear(); }

}  // namespace legacyalpha
