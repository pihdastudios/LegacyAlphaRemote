#ifndef LEGACY_ALPHA_REMOTE_TYPES_H
#define LEGACY_ALPHA_REMOTE_TYPES_H

#include <stdint.h>
#include <string>

namespace legacyalpha {

enum RemoteState {
    STATE_STOPPED,
    STATE_STARTING,
    STATE_ARMED,
    STATE_FOCUSING,
    STATE_COUNTDOWN,
    STATE_CAPTURING,
    STATE_COOLDOWN,
    STATE_ERROR
};

enum CommandType {
    COMMAND_START_AUTOFOCUS = 1,
    COMMAND_CANCEL_AUTOFOCUS = 2,
    COMMAND_CAPTURE = 3,
    COMMAND_RELEASE_SHUTTER = 4,
    COMMAND_RESTART_PREVIEW = 5,
    COMMAND_UPDATE_SCREEN = 6
};

struct NativeConfig {
    size_t maxRequestLine;
    size_t maxHeaders;
    size_t maxBody;
    size_t maxClients;
    int idleTimeoutMs;
    int maxDelayMs;
    int dedupeRetentionMs;
    int autofocusLeadMs;
    int immediateAutofocusLeadMs;
    int webShutterHoldMs;
    int physicalShutterFailsafeMs;
    int cooldownMs;
    int port;

    NativeConfig();
};

struct NativeCommand {
    int64_t id;
    CommandType type;
    int argument;
    std::string text;

    NativeCommand() : id(0), type(COMMAND_UPDATE_SCREEN), argument(0) {}
};

const char *stateName(RemoteState state);

}  // namespace legacyalpha

#endif
