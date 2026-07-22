#include "command_queue.h"

namespace legacyalpha {

CommandQueue::CommandQueue(size_t maximum) : maximum_(maximum) {}
bool CommandQueue::push(const NativeCommand& command) {
    if (commands_.size() >= maximum_)
        return false;
    commands_.push_back(command);
    return true;
}
bool CommandQueue::pop(NativeCommand* command) {
    if (commands_.empty())
        return false;
    *command = commands_.front();
    commands_.pop_front();
    return true;
}
void CommandQueue::clear() {
    commands_.clear();
}

} // namespace legacyalpha
