#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
WORKSPACE_DIR="$(cd "$PROJECT_DIR/.." && pwd)"
TOOLCHAIN_DIR="${SONY_TOOLCHAIN_DIR:-$WORKSPACE_DIR/toolchain}"
JDK8_HOME="${SONY_JAVA_HOME:-/home/klm/.package-manager/jdk}"
GRADLE_BIN="$TOOLCHAIN_DIR/gradle-2.14.1/bin/gradle"
GRADLE_INIT_SCRIPT="$TOOLCHAIN_DIR/legacy-repositories.gradle"
ANDROID_SDK_ROOT="$TOOLCHAIN_DIR/android-sdk"
ANDROID_NDK_ROOT="$TOOLCHAIN_DIR/android-ndk-r14b"
ANDROID_USER_ROOT="$TOOLCHAIN_DIR/android-user-home"
GRADLE_CACHE_ROOT="$TOOLCHAIN_DIR/gradle-user-home-2.14.1"
PMCA_PYTHON="$TOOLCHAIN_DIR/pmca-venv/bin/python"
PMCA_CONSOLE="$TOOLCHAIN_DIR/Sony-PMCA-RE/pmca-console.py"
APK_DIR="$PROJECT_DIR/app/build/outputs/apk"
require_file() { [[ -f "$1" ]] || { echo "Missing required file: $1" >&2; exit 1; }; }
require_dir() { [[ -d "$1" ]] || { echo "Missing required directory: $1" >&2; exit 1; }; }
latest_apk() { find "$APK_DIR" -maxdepth 1 -type f -name '*.apk' -printf '%T@ %p\n' | sort -nr | awk 'NR==1{sub(/^[^ ]+ /,""); print}' | grep .; }
export JAVA_HOME="$JDK8_HOME" ANDROID_HOME="$ANDROID_SDK_ROOT" ANDROID_SDK_HOME="$ANDROID_USER_ROOT" ANDROID_NDK_HOME="$ANDROID_NDK_ROOT" GRADLE_USER_HOME="$GRADLE_CACHE_ROOT"
export PATH="$JDK8_HOME/bin:$PATH"
