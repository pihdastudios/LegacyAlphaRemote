#include "remote_state_machine.h"
#include "native_log.h"

#include <stdio.h>

namespace legacyalpha {

RemoteStateMachine::RemoteStateMachine(const NativeConfig& config, const Clock& clock,
                                       CommandSink& sink)
    : config_(config), clock_(clock), sink_(sink), state_(STATE_STOPPED), running_(false),
      cameraReady_(false), wifiReady_(false), clientCount_(0), nextCommandId_(1),
      focusCommandId_(0), captureCommandId_(0), releaseCommandId_(0), focusDeadlineMs_(0),
      captureDeadlineMs_(0), releaseDeadlineMs_(0), cooldownDeadlineMs_(0),
      autofocusRequested_(false), focusSent_(false), captureSent_(false), physicalCapture_(false),
      standaloneFocus_(false) {}

void RemoteStateMachine::transition(RemoteState next) {
    if (state_ == next)
        return;
    std::string message("state ");
    message += stateName(state_);
    message += " -> ";
    message += stateName(next);
    nativeLog("INFO", message);
    state_ = next;
}

void RemoteStateMachine::start() {
    if (running_)
        return;
    running_ = true;
    lastError_.clear();
    clearSchedule();
    transition(STATE_STARTING);
    if (cameraReady_ && wifiReady_)
        transition(STATE_ARMED);
}

void RemoteStateMachine::stop() {
    if (!running_ && state_ == STATE_STOPPED)
        return;
    if (focusSent_)
        send(COMMAND_CANCEL_AUTOFOCUS, 0, "", NULL);
    clearSchedule();
    running_ = false;
    transition(STATE_STOPPED);
}

void RemoteStateMachine::setCameraReady(bool ready) {
    cameraReady_ = ready;
    if (!running_)
        return;
    if (!ready) {
        clearSchedule();
        transition(STATE_STARTING);
    } else if (wifiReady_ && state_ == STATE_STARTING)
        transition(STATE_ARMED);
}

void RemoteStateMachine::setWifiReady(bool ready) {
    wifiReady_ = ready;
    if (!running_)
        return;
    if (!ready) {
        clearSchedule();
        transition(STATE_STARTING);
    } else if (cameraReady_ && state_ == STATE_STARTING)
        transition(STATE_ARMED);
}

bool RemoteStateMachine::acceptingRequests() const {
    return running_ && cameraReady_ && wifiReady_ && state_ != STATE_STOPPED;
}

bool RemoteStateMachine::send(CommandType type, int argument, const std::string& text,
                              int64_t* commandId) {
    NativeCommand command;
    command.id = nextCommandId_++;
    command.type = type;
    command.argument = argument;
    command.text = text;
    if (!sink_.dispatch(command)) {
        fail("native command queue full");
        return false;
    }
    if (commandId != NULL)
        *commandId = command.id;
    return true;
}

bool RemoteStateMachine::requestCapture(const std::string& requestId, int delayMs, bool autofocus,
                                        std::string* error) {
    if (!acceptingRequests() || state_ != STATE_ARMED) {
        *error = "remote is busy";
        return false;
    }
    if (delayMs < 0 || delayMs > config_.maxDelayMs) {
        *error = "delay_ms is out of range";
        return false;
    }
    clearSchedule();
    const int64_t now = clock_.nowMs();
    lastRequestId_ = requestId;
    lastError_.clear();
    autofocusRequested_ = autofocus;
    captureDeadlineMs_ = now + delayMs;
    focusDeadlineMs_ = autofocus ? captureDeadlineMs_ - config_.autofocusLeadMs : 0;
    if (delayMs == 0 && autofocus) {
        captureDeadlineMs_ = now + config_.immediateAutofocusLeadMs;
        focusDeadlineMs_ = now;
    }
    if (autofocus && focusDeadlineMs_ < now)
        focusDeadlineMs_ = now;
    transition(delayMs > 0 ? STATE_COUNTDOWN : (autofocus ? STATE_FOCUSING : STATE_COUNTDOWN));
    tick();
    return state_ != STATE_ERROR;
}

bool RemoteStateMachine::cancelCountdown(std::string* error) {
    if (captureSent_ || (state_ != STATE_COUNTDOWN && state_ != STATE_FOCUSING)) {
        *error = "countdown cannot be cancelled";
        return false;
    }
    if (focusSent_)
        send(COMMAND_CANCEL_AUTOFOCUS, 0, "", NULL);
    nativeLog("INFO", "countdown cancelled");
    clearSchedule();
    transition(cameraReady_ && wifiReady_ ? STATE_ARMED : STATE_STARTING);
    return true;
}

bool RemoteStateMachine::setFocus(bool pressed, std::string* error) {
    if (pressed) {
        if (state_ != STATE_ARMED) {
            *error = "remote is busy";
            return false;
        }
        standaloneFocus_ = true;
        focusSent_ = true;
        if (!send(COMMAND_START_AUTOFOCUS, 0, "", &focusCommandId_))
            return false;
        transition(STATE_FOCUSING);
        return true;
    }
    if (!standaloneFocus_ || state_ != STATE_FOCUSING) {
        *error = "focus is not active";
        return false;
    }
    send(COMMAND_CANCEL_AUTOFOCUS, 0, "", NULL);
    clearSchedule();
    transition(cameraReady_ && wifiReady_ ? STATE_ARMED : STATE_STARTING);
    return true;
}

bool RemoteStateMachine::physicalShutter(bool pressed, std::string* error) {
    if (!pressed) {
        if (state_ == STATE_CAPTURING && physicalCapture_ && releaseDeadlineMs_ > 0) {
            releaseDeadlineMs_ = clock_.nowMs();
            tick();
        }
        return true;
    }
    if (!running_ || !cameraReady_) {
        *error = "camera is not ready";
        return false;
    }
    if (state_ == STATE_COUNTDOWN || (state_ == STATE_FOCUSING && !standaloneFocus_)) {
        if (focusSent_)
            send(COMMAND_CANCEL_AUTOFOCUS, 0, "", NULL);
        clearSchedule();
    } else if (state_ != STATE_ARMED && !(state_ == STATE_FOCUSING && standaloneFocus_)) {
        *error = "camera is busy";
        return false;
    }
    lastRequestId_ = "physical";
    lastError_.clear();
    beginCapture(true);
    return state_ != STATE_ERROR;
}

void RemoteStateMachine::beginCapture(bool physical) {
    physicalCapture_ = physical;
    captureSent_ = true;
    standaloneFocus_ = false;
    if (!send(COMMAND_CAPTURE, 0, "", &captureCommandId_))
        return;
    const int64_t now = clock_.nowMs();
    releaseDeadlineMs_ =
        now + (physical ? config_.physicalShutterFailsafeMs : config_.webShutterHoldMs);
    cooldownDeadlineMs_ = now + config_.cooldownMs;
    transition(STATE_CAPTURING);
}

void RemoteStateMachine::tick() {
    if (!running_)
        return;
    const int64_t now = clock_.nowMs();
    if ((state_ == STATE_COUNTDOWN || state_ == STATE_FOCUSING) && autofocusRequested_ &&
        !focusSent_ && now >= focusDeadlineMs_) {
        focusSent_ = true;
        if (!send(COMMAND_START_AUTOFOCUS, 0, "", &focusCommandId_))
            return;
        transition(STATE_FOCUSING);
    }
    if ((state_ == STATE_COUNTDOWN || state_ == STATE_FOCUSING) && !captureSent_ &&
        now >= captureDeadlineMs_)
        beginCapture(false);
    if (state_ == STATE_CAPTURING && captureSent_ && releaseDeadlineMs_ > 0 &&
        now >= releaseDeadlineMs_) {
        releaseDeadlineMs_ = 0;
        if (!send(COMMAND_RELEASE_SHUTTER, 0, "", &releaseCommandId_))
            return;
        transition(STATE_COOLDOWN);
    }
    if (state_ == STATE_COOLDOWN && now >= cooldownDeadlineMs_) {
        clearSchedule();
        transition(cameraReady_ && wifiReady_ ? STATE_ARMED : STATE_STARTING);
    }
}

void RemoteStateMachine::reportCommandResult(int64_t commandId, int resultCode,
                                             const std::string& message) {
    if (commandId == captureCommandId_ && resultCode != 0) {
        fail(message.empty() ? "capture failed" : message);
        return;
    }
    if (commandId == focusCommandId_ && resultCode != 0 && !standaloneFocus_) {
        if (!captureSent_) {
            lastError_ = message.empty() ? "autofocus failed" : message;
            clearSchedule();
            transition(STATE_ARMED);
        }
    }
    if (commandId == releaseCommandId_ && resultCode != 0) {
        lastError_ = message.empty() ? "shutter release failed" : message;
    }
}

int RemoteStateMachine::countdownRemainingMs() const {
    if ((state_ != STATE_COUNTDOWN && state_ != STATE_FOCUSING) || captureDeadlineMs_ <= 0)
        return 0;
    int64_t remaining = captureDeadlineMs_ - clock_.nowMs();
    return remaining > 0 ? static_cast<int>(remaining) : 0;
}

void RemoteStateMachine::setClientCount(int count) {
    clientCount_ = count < 0 ? 0 : count;
}

void RemoteStateMachine::clearSchedule() {
    focusCommandId_ = captureCommandId_ = releaseCommandId_ = 0;
    focusDeadlineMs_ = captureDeadlineMs_ = releaseDeadlineMs_ = cooldownDeadlineMs_ = 0;
    autofocusRequested_ = focusSent_ = captureSent_ = physicalCapture_ = standaloneFocus_ = false;
}

void RemoteStateMachine::fail(const std::string& error) {
    lastError_ = error;
    nativeLog("ERROR", error);
    clearSchedule();
    transition(STATE_ERROR);
}

} // namespace legacyalpha
