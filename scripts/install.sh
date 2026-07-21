#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; source "$SCRIPT_DIR/_env.sh"
apk="${1:-$(latest_apk)}"; [[ "$apk" = /* ]] || apk="$(cd "$(dirname "$apk")" && pwd)/$(basename "$apk")"
require_file "$PMCA_PYTHON"; require_file "$PMCA_CONSOLE"; "$SCRIPT_DIR/usb-status.sh"; "$SCRIPT_DIR/verify-apk.sh" "$apk"
(cd "$WORKSPACE_DIR" && "$PMCA_PYTHON" "$PMCA_CONSOLE" install -f "$apk")
