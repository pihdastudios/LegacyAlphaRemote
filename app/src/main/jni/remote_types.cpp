#include "remote_types.h"

namespace legacyalpha {

NativeConfig::NativeConfig()
    : maxRequestLine(2048), maxHeaders(8192), maxBody(4096), maxClients(4), idleTimeoutMs(5000),
      maxDelayMs(10000), dedupeRetentionMs(60000), autofocusLeadMs(1000),
      immediateAutofocusLeadMs(500), webShutterHoldMs(400), physicalShutterFailsafeMs(800),
      cooldownMs(3000), port(8080) {}

const char* stateName(RemoteState state) {
    switch (state) {
        case STATE_STOPPED:
            return "stopped";
        case STATE_STARTING:
            return "starting";
        case STATE_ARMED:
            return "armed";
        case STATE_FOCUSING:
            return "focusing";
        case STATE_COUNTDOWN:
            return "countdown";
        case STATE_CAPTURING:
            return "capturing";
        case STATE_COOLDOWN:
            return "cooldown";
        case STATE_ERROR:
            return "error";
    }
    return "error";
}

} // namespace legacyalpha
