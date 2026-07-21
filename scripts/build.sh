#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/_env.sh"
tasks=(); offline=true
while [[ $# -gt 0 ]]; do
  case "$1" in --online) offline=false;; --clean) tasks+=(clean);; *) tasks+=("$1");; esac; shift
done
[[ ${#tasks[@]} -gt 0 ]] || tasks+=(assembleDebug)
require_file "$GRADLE_BIN"; require_file "$GRADLE_INIT_SCRIPT"; require_dir "$ANDROID_SDK_ROOT"; require_dir "$ANDROID_NDK_ROOT"
args=(--no-daemon --init-script "$GRADLE_INIT_SCRIPT"); [[ "$offline" == true ]] && args+=(--offline)
# NDK r14b's clang requires the legacy ncurses SONAMEs on modern Linux hosts.
compat="${LEGACY_NCURSES_DIR:-/tmp/legacyalpha-libs}"
if [[ -d "$compat" ]]; then export LD_LIBRARY_PATH="$compat${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"; fi
(cd "$PROJECT_DIR" && "$GRADLE_BIN" "${args[@]}" "${tasks[@]}")
if printf '%s\n' "${tasks[@]}" | grep -qx assembleDebug; then "$SCRIPT_DIR/verify-apk.sh"; fi
