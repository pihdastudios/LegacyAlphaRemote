#include "http_parser.h"
#include "remote_state_machine.h"

#include <assert.h>
#include <stdio.h>
#include <vector>

using namespace legacyalpha;

class TestClock : public Clock {
  public:
    int64_t value;
    TestClock() : value(0) {}
    virtual int64_t nowMs() const {
        return value;
    }
};

class TestSink : public CommandSink {
  public:
    std::vector<NativeCommand> commands;
    virtual bool dispatch(const NativeCommand& command) {
        commands.push_back(command);
        return true;
    }
};

int main() {
    NativeConfig config;
    HttpRequest request;
    size_t consumed = 0;
    const std::string body = "{\"request_id\":\"abc-1\",\"delay_ms\":3000,\"autofocus\":true}";
    char header[256];
    snprintf(header, sizeof(header),
             "POST /api/v1/capture HTTP/1.1\r\nContent-Length: %lu\r\nContent-Type: "
             "application/json\r\nX-LegacyAlpha-Pin: 123456\r\n\r\n",
             static_cast<unsigned long>(body.size()));
    const std::string raw = std::string(header) + body;
    assert(parseHttpRequest(raw, config, &request, &consumed) == PARSE_OK);
    CaptureRequest capture;
    assert(parseCaptureJson(request.body, &capture));
    assert(capture.requestId == "abc-1" && capture.delayMs == 3000 && capture.autofocus);
    assert(parseHttpRequest("GET /../secret HTTP/1.0\r\n\r\n", config, &request, &consumed) ==
           PARSE_BAD_REQUEST);

    TestClock clock;
    TestSink sink;
    RemoteStateMachine machine(config, clock, sink);
    machine.setCameraReady(true);
    machine.setWifiReady(true);
    machine.start();
    std::string error;
    assert(machine.requestCapture("abc-1", 1000, true, &error));
    assert(machine.state() == STATE_FOCUSING);
    assert(sink.commands.size() == 1 && sink.commands[0].type == COMMAND_START_AUTOFOCUS);
    clock.value = 1000;
    machine.tick();
    assert(sink.commands.size() == 2 && sink.commands[1].type == COMMAND_CAPTURE);
    clock.value = 1400;
    machine.tick();
    assert(sink.commands.size() == 3 && sink.commands[2].type == COMMAND_RELEASE_SHUTTER);
    clock.value = 4400;
    machine.tick();
    assert(machine.state() == STATE_ARMED);
    puts("native remote tests passed");
    return 0;
}
