#include "monotonic_clock.h"

#include <time.h>

namespace legacyalpha {

int64_t MonotonicClock::nowMs() const {
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0)
        return 0;
    return static_cast<int64_t>(value.tv_sec) * 1000 + value.tv_nsec / 1000000;
}

} // namespace legacyalpha
