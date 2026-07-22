#ifndef LEGACY_ALPHA_REMOTE_STATE_MACHINE_H
#define LEGACY_ALPHA_REMOTE_STATE_MACHINE_H

#include "monotonic_clock.h"
#include "remote_types.h"

#include <string>

namespace legacyalpha {

class CommandSink {
  public:
    virtual ~CommandSink() {}
    virtual bool dispatch(const NativeCommand& command) = 0;
};

class RemoteStateMachine {
  public:
    RemoteStateMachine(const NativeConfig& config, const Clock& clock, CommandSink& sink);
    void start();
    void stop();
    void setCameraReady(bool ready);
    void setWifiReady(bool ready);
    bool requestCapture(const std::string& requestId, int delayMs, bool autofocus,
                        std::string* error);
    bool cancelCountdown(std::string* error);
    bool setFocus(bool pressed, std::string* error);
    bool physicalShutter(bool pressed, std::string* error);
    void tick();
    void reportCommandResult(int64_t commandId, int resultCode, const std::string& message);
    void setClientCount(int count);

    RemoteState state() const {
        return state_;
    }
    bool cameraReady() const {
        return cameraReady_;
    }
    bool wifiReady() const {
        return wifiReady_;
    }
    int clientCount() const {
        return clientCount_;
    }
    int countdownRemainingMs() const;
    const std::string& lastRequestId() const {
        return lastRequestId_;
    }
    const std::string& lastError() const {
        return lastError_;
    }
    bool acceptingRequests() const;

  private:
    void transition(RemoteState next);
    bool send(CommandType type, int argument, const std::string& text, int64_t* commandId);
    void beginCapture(bool physical);
    void clearSchedule();
    void fail(const std::string& error);

    NativeConfig config_;
    const Clock& clock_;
    CommandSink& sink_;
    RemoteState state_;
    bool running_;
    bool cameraReady_;
    bool wifiReady_;
    int clientCount_;
    int64_t nextCommandId_;
    int64_t focusCommandId_;
    int64_t captureCommandId_;
    int64_t releaseCommandId_;
    int64_t focusDeadlineMs_;
    int64_t captureDeadlineMs_;
    int64_t releaseDeadlineMs_;
    int64_t cooldownDeadlineMs_;
    bool autofocusRequested_;
    bool focusSent_;
    bool captureSent_;
    bool physicalCapture_;
    bool standaloneFocus_;
    std::string lastRequestId_;
    std::string lastError_;
};

} // namespace legacyalpha

#endif
