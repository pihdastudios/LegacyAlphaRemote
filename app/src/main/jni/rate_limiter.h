#ifndef LEGACY_ALPHA_RATE_LIMITER_H
#define LEGACY_ALPHA_RATE_LIMITER_H

#include <stdint.h>
#include <string>
#include <vector>

namespace legacyalpha {

class RateLimiter {
  public:
    RateLimiter(double capacity, double refillPerSecond, size_t maximumKeys);
    bool allow(const std::string& key, int64_t nowMs);
    void clear();

  private:
    struct Bucket {
        std::string key;
        double tokens;
        int64_t lastMs;
    };
    double capacity_;
    double refillPerMs_;
    size_t maximumKeys_;
    std::vector<Bucket> buckets_;
};

} // namespace legacyalpha

#endif
