#ifndef LEGACY_ALPHA_MONOTONIC_CLOCK_H
#define LEGACY_ALPHA_MONOTONIC_CLOCK_H

#include <stdint.h>

namespace legacyalpha {

class Clock {
  public:
    virtual ~Clock() {}
    virtual int64_t nowMs() const = 0;
};

class MonotonicClock : public Clock {
  public:
    virtual int64_t nowMs() const;
};

} // namespace legacyalpha

#endif
