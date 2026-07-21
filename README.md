# Legacy Alpha Remote

Legacy Alpha Remote is a Sony PMCADemo/OpenMemories Android 10 application for the Alpha 6000-era camera. It starts Sony Wi-Fi Direct, displays the group credentials and a session PIN on the camera, and serves a local browser shutter UI at `http://192.168.122.1:8080/` (or the address reported by the camera).

The native service deliberately stays small and bounded: four clients, bounded request/header/body buffers, HTTP/1.0-style close responses, an allowlisted JSON API, per-client token-bucket rate limits, constant-time PIN comparison, request-id deduplication, and a single serialized camera command queue. The web UI is embedded in the native library, so no writable storage is used for web assets.

Build with the repository's pinned Gradle 2.14.1, Android SDK API 10/build-tools 25.0.2, and NDK r14b toolchain. The host parser/state-machine regression test can be run with:

```sh
g++ -std=c++11 -pthread -I app/src/main/jni \
  app/src/test/native/remote_native_test.cpp app/src/main/jni/*.cpp \
  -o /tmp/legacyalpha-native-test
```

Exclude `legacyalpharemote_jni.cpp` from that host command unless JNI headers are available.
