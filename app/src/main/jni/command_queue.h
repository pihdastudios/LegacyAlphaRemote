#ifndef LEGACY_ALPHA_COMMAND_QUEUE_H
#define LEGACY_ALPHA_COMMAND_QUEUE_H

#include "remote_types.h"

#include <deque>

namespace legacyalpha {

class CommandQueue {
public:
    explicit CommandQueue(size_t maximum);
    bool push(const NativeCommand &command);
    bool pop(NativeCommand *command);
    void clear();
private:
    size_t maximum_;
    std::deque<NativeCommand> commands_;
};

}  // namespace legacyalpha

#endif
