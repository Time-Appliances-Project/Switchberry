#!/usr/bin/env bash
set -euo pipefail

# switchberry_cm4_pps_monitor.sh
#
# Intent:
#   Gate the CM4 NIC PHC PPS *output* (PEROUT) based on whether the ptp4l client
#   appears to be tightly locked to its master.
#
#   Enable PPS output only when ALL of these are true:
#     1) The ptp4l client systemd unit is running
#     2) ptp4l is currently in servo state s2
#     3) The last N "master offset" samples (in s2) are within a threshold
#
#   If any condition stops being true, disable PPS output and print WHY.

# ---- Tunables (override via environment) ----
CLIENT_UNIT="${CLIENT_UNIT:-ptp4l-switchberry-client.service}"
PTP_DEVICE="${PTP_DEVICE:-/dev/ptp0}"
TESTPTP_BIN="${TESTPTP_BIN:-testptp}"

# How often to re-check lock status
POLL_INTERVAL_SEC="${POLL_INTERVAL_SEC:-2}"

# How many journal lines to scan each poll
JOURNAL_LINES="${JOURNAL_LINES:-200}"

# How many recent offset samples must be "tight"
OFFSET_SAMPLES="${OFFSET_SAMPLES:-10}"

# "four digit lock" => abs(offset) <= 9999 ns by default
OFFSET_ABS_MAX_NS="${OFFSET_ABS_MAX_NS:-500}"

# PEROUT configuration (testptp)
PPS_OUT_CH="${PPS_OUT_CH:-0}"
PPS_PERIOD_NS="${PPS_PERIOD_NS:-1000000000}"   # 1 Hz

log() {
  echo "[$(date -Is)] $*" >&2
}

# Return 0 if the systemd unit is active.
client_running() {
  systemctl is-active --quiet "$CLIENT_UNIT"
}

# Get the MainPID of the running service. Returns 0 or empty if not running.
get_client_pid() {
  systemctl show --property MainPID --value "$CLIENT_UNIT"
}

# Extract the most recent servo state from ptp4l logs (s0/s1/s2).
# Prints "s2" etc. or empty if not found.
current_servo_state() {
  local pid
  pid="$(get_client_pid)"
  if [[ -z "$pid" || "$pid" == "0" ]]; then return; fi
  
  journalctl _PID="$pid" -n "$JOURNAL_LINES" -o cat 2>/dev/null \
    | grep -E 'master offset' \
    | tail -n 1 \
    | sed -nE 's/.*master offset[[:space:]]+-?[0-9]+[[:space:]]+(s[0-9]+)[[:space:]]+freq.*/\1/p'
}

# Print the last N "master offset" values (ns) from s2 lines, one per line.
last_s2_offsets_ns() {
  local n="$1"
  local pid
  pid="$(get_client_pid)"
  if [[ -z "$pid" || "$pid" == "0" ]]; then return; fi

  journalctl _PID="$pid" -n "$JOURNAL_LINES" -o cat 2>/dev/null \
    | sed -nE 's/.*master offset[[:space:]]+(-?[0-9]+)[[:space:]]+s2[[:space:]]+freq.*/\1/p' \
    | tail -n "$n"
}

enable_pps() {
  "$TESTPTP_BIN" -d "$PTP_DEVICE" -L "${PPS_OUT_CH},2" >/dev/null 2>&1
  "$TESTPTP_BIN" -d "$PTP_DEVICE" -p "$PPS_PERIOD_NS"  >/dev/null 2>&1
}

disable_pps() {
  "$TESTPTP_BIN" -d "$PTP_DEVICE" -L "${PPS_OUT_CH},0" >/dev/null 2>&1 || true
}

# Sets FAIL_REASON and returns:
#   0 => OK to enable PPS
#   1 => Not OK (FAIL_REASON explains why)
check_conditions() {
  FAIL_REASON=""

  # 1) Is the service running?
  if ! client_running; then
    FAIL_REASON="client unit not running: $CLIENT_UNIT"
    return 1
  fi

  # 2) Is the servo in s2?
  local st
  st="$(current_servo_state)"
  if [[ "$st" != "s2" ]]; then
    FAIL_REASON="not in s2 (last servo state='${st:-<none>}')"
    return 1
  fi

  # 3) Are the last N offsets small enough?
  local -a offs
  mapfile -t offs < <(last_s2_offsets_ns "$OFFSET_SAMPLES" || true)

  if (( ${#offs[@]} < OFFSET_SAMPLES )); then
    FAIL_REASON="not enough s2 offset samples (have ${#offs[@]}, need $OFFSET_SAMPLES)"
    return 1
  fi

  local s a
  for s in "${offs[@]}"; do
    a=$s; (( a < 0 )) && a=$(( -a ))
    if (( a > OFFSET_ABS_MAX_NS )); then
      FAIL_REASON="offset too large: ${s} ns (limit ±${OFFSET_ABS_MAX_NS} ns); recent=(${offs[*]})"
      return 1
    fi
  done

  return 0
}

# ---- Startup sanity checks ----
if ! command -v "$TESTPTP_BIN" >/dev/null 2>&1; then
  log "ERROR: $TESTPTP_BIN not found in PATH"
  exit 1
fi

if [[ ! -e "$PTP_DEVICE" ]]; then
  log "ERROR: PTP device $PTP_DEVICE not found"
  exit 1
fi

pps_enabled=0
FAIL_REASON=""

# Ensure clean state at startup (disable potentially leftover PPS)
disable_pps

# Always try to disable PPS when exiting (best-effort).
cleanup() {
  if (( pps_enabled == 1 )); then
    disable_pps
  fi
}
trap cleanup EXIT INT TERM

log "Starting PPS monitor: unit=$CLIENT_UNIT dev=$PTP_DEVICE poll=${POLL_INTERVAL_SEC}s samples=$OFFSET_SAMPLES thr=±${OFFSET_ABS_MAX_NS}ns"

# ---- Main loop ----
while true; do
  if check_conditions; then
    if (( pps_enabled == 0 )); then
      log "Conditions met: enabling PPS output (PEROUT) ch=$PPS_OUT_CH period=${PPS_PERIOD_NS}ns; recent offsets: $(last_s2_offsets_ns "$OFFSET_SAMPLES" | tr '\n' ' ' | sed 's/[[:space:]]\+$//')"
      if enable_pps; then
        pps_enabled=1
        log "PPS output enabled"
      else
        log "ERROR: failed to enable PPS output via testptp"
      fi
    fi
  else
    if (( pps_enabled == 1 )); then
      log "Conditions failed: disabling PPS output: $FAIL_REASON"
      disable_pps
      pps_enabled=0
      log "PPS output disabled"
    fi
  fi

  sleep "$POLL_INTERVAL_SEC"
done

