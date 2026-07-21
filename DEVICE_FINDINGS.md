# Legacy Alpha Remote ILCE-5100 Findings

Last updated: 2026-07-22. Treat only entries explicitly marked tested as device results.

## Tested hardware and toolchain

- Camera: Sony ILCE-5100 in Sony-PMCA-compatible Mass Storage mode (`054c:07cd`).
- Build: JDK 8, Gradle 2.14.1, Android API/target 10, Build Tools 25.0.2, NDK r14b, and `armeabi` only.
- Installation: Sony-PMCA-RE accepts and installs the signed debug APK.

## Device-tested behavior

- The application opens `CameraEx`, starts normal preview, creates the Sony Wi-Fi Direct group, displays connection credentials and a six-digit session PIN, and serves the phone UI on port 8080.
- Some Sony Wi-Fi configuration data presented an interface/MAC-like value rather than a browser-usable IPv4 address. Rejecting non-IPv4 values, advertising the standard `192.168.122.1` fallback, and using a defensive local listener bind produced a working phone page.
- The first embedded phone page loaded HTML but remained at `Connecting...` because C++ `\n` escapes became literal newlines inside a JavaScript quoted string. Double escaping the embedded source and checking the emitted script with `node --check` fixed polling.
- PIN-authenticated remote capture works from the phone interface. The native service uses bounded clients, request buffers, queues, rate limits, request-id deduplication, monotonic timing, and a serialized camera state machine.
- Pressing the camera DISP/up key hides and restores the diagnostics overlay, leaving normal preview unobstructed while retaining setup information on demand.
- The phone interface displays the newest bounded thumbnail after capture. It prefers the Sony captured-JPEG callback and labels a one-shot live-view image when the five-second fallback is used. Only one thumbnail is retained and access requires the session PIN.
- The current APK builds, passes API/ABI/signature verification, and installs successfully through `scripts/build-install.sh`.

## Operational details

- Default phone URL: `http://192.168.122.1:8080/`, unless the camera reports another validated IPv4 address.
- Camera log: `/sdcard/LEGACYALPHAREMOTE/LOG.TXT`.
- Capture uses the established `takePicture(null, null, null)` path, followed by a guarded shutter release and preview recovery.
- The diagnostics overlay is visible at launch so the SSID, Wi-Fi password, URL, and PIN can be read before it is hidden.

## Unresolved questions

- Capture-JPEG callback timing and frequency should be measured from collected logs across repeated captures; the fallback exists because Sony firmware behavior is not fully documented.
- Repeated long sessions should be checked for thumbnail decode memory pressure, stale preview versions, Wi-Fi reconnect behavior, and clean activity pause/resume.
- Listener bind errors now include port and native errno details, but failure behavior with another service already occupying port 8080 remains untested.
