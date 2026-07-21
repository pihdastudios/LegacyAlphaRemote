#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; source "$SCRIPT_DIR/_env.sh"
apk="${1:-$(latest_apk)}"; [[ "$apk" = /* ]] || apk="$(cd "$(dirname "$apk")" && pwd)/$(basename "$apk")"
require_file "$apk"; aapt="$ANDROID_SDK_ROOT/build-tools/25.0.2/aapt"; signer="$ANDROID_SDK_ROOT/build-tools/25.0.2/apksigner"; require_file "$aapt"; require_file "$signer"
badging="$($aapt dump badging "$apk")"; grep -q "sdkVersion:'10'" <<<"$badging"; grep -q "targetSdkVersion:'10'" <<<"$badging"
unexpected="$(unzip -Z1 "$apk" | awk '/^lib\// && $0 !~ /^lib\/armeabi\// {print}')"; [[ -z "$unexpected" ]] || { echo "$unexpected" >&2; exit 1; }
PATH="$JDK8_HOME/bin:/usr/bin:/bin" "$signer" verify --verbose "$apk"; sha256sum "$apk"; echo "$badging" | sed -n '1,12p'; echo "Verified APK: $apk"
