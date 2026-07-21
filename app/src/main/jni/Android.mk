LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := legacyalpharemote
LOCAL_SRC_FILES := command_queue.cpp embedded_web.cpp http_parser.cpp http_response.cpp \
    json_writer.cpp monotonic_clock.cpp native_log.cpp native_runtime.cpp \
    legacyalpharemote_jni.cpp rate_limiter.cpp remote_state_machine.cpp \
    remote_types.cpp request_deduplicator.cpp http_server.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)
LOCAL_CPPFLAGS := -Wall -Wextra -Werror=return-type -fno-exceptions -fno-rtti
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)
