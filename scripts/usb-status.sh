#!/usr/bin/env bash
set -euo pipefail
devices="$(lsusb | awk 'tolower($0) ~ /sony|054c:/ {print}')"; [[ -n "$devices" ]] || { echo "No Sony USB device detected" >&2; exit 1; }; echo "$devices"
grep -q '054c:0994' <<<"$devices" && { echo "Camera is in charging mode; select USB Connection -> Mass Storage." >&2; exit 2; }
grep -q '054c:07cd' <<<"$devices" && echo "ILCE-5100 detected in a PMCA-compatible USB mode."
