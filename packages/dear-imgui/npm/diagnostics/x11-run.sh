#!/usr/bin/env bash
set -euo pipefail
# Check before starting either process so a missing utility cannot masquerade as
# a window-manager timeout. xauth is required internally by xvfb-run.
fixture_missing=()
for prerequisite in xvfb-run Xvfb xauth openbox xprop xdotool wmctrl; do
  command -v "$prerequisite" >/dev/null 2>&1 || fixture_missing+=("$prerequisite")
done
if ((${#fixture_missing[@]})); then
  echo "Missing X11 fixture prerequisites: ${fixture_missing[*]}" >&2
  echo 'On Ubuntu: sudo apt-get install xvfb xauth x11-utils openbox xdotool wmctrl' >&2
  exit 1
fi
# An isolated display and window manager make focus, iconify and WM_DELETE
# actual application events without touching the host desktop session.
if [[ ${XFRAMES_X11_FIXTURE_SESSION:-0} != 1 ]]; then
  exec xvfb-run -a env XFRAMES_X11_FIXTURE_SESSION=1 bash "$0" "$@"
fi
fixture_log_dir=${XFRAMES_DIAGNOSTICS_DIR:-build/diagnostics/node}
mkdir -p "$fixture_log_dir"
openbox --sm-disable > "$fixture_log_dir/x11-window-manager.log" 2>&1 &
fixture_wm_pid=$!
trap 'kill "$fixture_wm_pid" 2>/dev/null || true; wait "$fixture_wm_pid" 2>/dev/null || true' EXIT
for ((attempt=0; attempt<100; attempt++)); do
  if xprop -root _NET_SUPPORTING_WM_CHECK | grep -Eq 'window id # 0x[1-9a-f]'; then
    "$@"
    exit $?
  fi
  kill -0 "$fixture_wm_pid"
  sleep 0.05
done
echo 'Fixture window manager did not become ready' >&2
exit 1
