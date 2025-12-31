#!/usr/bin/env bash
set -euo pipefail

DPLL_STATUS_FILE="${DPLL_STATUS_FILE:-/tmp/switchberry-clockmatrix.status}"
TS2PHC_STATUS_FILE="${TS2PHC_STATUS_FILE:-/tmp/switchberry-ts2phc.status}"

POLL_SEC="${POLL_SEC:-0.2}"

# Require ts2phc "OK" continuously for this long before starting ptp4l.
TS2PHC_OK_HOLD_SEC="${TS2PHC_OK_HOLD_SEC:-3}"

# What to do when DPLL is NOT_OK (intervening):
#   stop    (default, safest)
#   degrade (keep ptp4l running but advertise degraded GM quality)
PTP4L_NOT_OK_ACTION="${PTP4L_NOT_OK_ACTION:-stop}"

# Optional: allow ptp4l to run even if ts2phc is not converged (holdover GM),
# but only in "degrade" mode. Default off.
PTP4L_ALLOW_HOLDOVER="${PTP4L_ALLOW_HOLDOVER:-0}"

PTP4L_CMD=(/usr/sbin/ptp4l -f /etc/switchberry/ptp4l-switchberry-gm-uc.conf -m)

# pmc settings (only used if PTP4L_NOT_OK_ACTION=degrade)
PMC_SOCK="${PMC_SOCK:-/var/run/ptp4l}"
PMC_BASE=(pmc -u -b 0 -t 1 -s "$PMC_SOCK")

GOOD_CLOCKCLASS="${GOOD_CLOCKCLASS:-6}"
GOOD_CLOCKACCURACY="${GOOD_CLOCKACCURACY:-0x22}"
GOOD_OSLV="${GOOD_OSLV:-0x0001}"
GOOD_UTC_VALID="${GOOD_UTC_VALID:-1}"
GOOD_TIMETRACE="${GOOD_TIMETRACE:-1}"
GOOD_FREQTRACE="${GOOD_FREQTRACE:-1}"
GOOD_TIMESOURCE="${GOOD_TIMESOURCE:-0x20}"   # gps

DEG_CLOCKCLASS="${DEG_CLOCKCLASS:-248}"
DEG_CLOCKACCURACY="${DEG_CLOCKACCURACY:-0xfe}"
DEG_OSLV="${DEG_OSLV:-0xffff}"
DEG_UTC_VALID="${DEG_UTC_VALID:-0}"
DEG_TIMETRACE="${DEG_TIMETRACE:-0}"
DEG_FREQTRACE="${DEG_FREQTRACE:-0}"
DEG_TIMESOURCE="${DEG_TIMESOURCE:-0xa0}"     # internal oscillator

ts() { date +"%Y-%m-%d %H:%M:%S"; }
log() { echo "[$(ts)] ptp4l-gm-guard: $*"; }

read_first_line() {
  local file="$1"
  if [[ -f "$file" ]]; then
    head -n1 "$file" 2>/dev/null || echo "NOT_OK"
  else
    echo "NOT_OK"
  fi
}

dpll_ok()   { [[ "$(read_first_line "$DPLL_STATUS_FILE")" == "OK" ]]; }
ts2phc_ok() { [[ "$(read_first_line "$TS2PHC_STATUS_FILE")" == "OK" ]]; }

# ptp4l process group
ptp4l_pgid=0
stop_requested=0

cleanup() {
  stop_requested=1
  if (( ptp4l_pgid != 0 )); then
    log "Stopping ptp4l (pgid=$ptp4l_pgid)"
    kill -TERM "-$ptp4l_pgid" 2>/dev/null || true
    sleep 0.2
    kill -KILL "-$ptp4l_pgid" 2>/dev/null || true
  fi
  exit 0
}
trap cleanup TERM INT

wait_for_pmc_ready() {
  # Best-effort: only used in degrade mode.
  while true; do
    if "${PMC_BASE[@]}" "GET PORT_DATA_SET" >/dev/null 2>&1; then
      return 0
    fi
    sleep 0.2
  done
}

pmc_set_gm_np() {
  # Base on current GRANDMASTER_SETTINGS_NP so we preserve utc offset + leap flags + ptpTimescale
  local base
  base="$("${PMC_BASE[@]}" "GET GRANDMASTER_SETTINGS_NP" 2>/dev/null \
    | awk 'NF>=2 {print $1, $2}' \
    | grep -E '^(clockClass|clockAccuracy|offsetScaledLogVariance|currentUtcOffset|leap61|leap59|currentUtcOffsetValid|ptpTimescale|timeTraceable|frequencyTraceable|timeSource) ' \
    | awk '{printf " %s %s", $1, $2}' || true)"

  if [[ -z "$base" ]]; then
    base=" clockClass 248 clockAccuracy 0xfe offsetScaledLogVariance 0xffff currentUtcOffset 37 leap61 0 leap59 0 currentUtcOffsetValid 0 ptpTimescale 1 timeTraceable 0 frequencyTraceable 0 timeSource 0xa0"
  fi

  # apply overrides passed as args: key val key val...
  local kv="$base"
  while (( "$#" >= 2 )); do
    local k="$1" v="$2"; shift 2
    if echo "$kv" | grep -Eq "(^| )${k} "; then
      kv="$(echo "$kv" | sed -E "s/(^| )(${k}) [^ ]+/\1\2 ${v}/")"
    else
      kv="${kv} ${k} ${v}"
    fi
  done

  # shellcheck disable=SC2086
  "${PMC_BASE[@]}" "SET GRANDMASTER_SETTINGS_NP${kv}" >/dev/null
}

apply_quality() {
  local mode="$1" # GOOD | DEGRADED
  if [[ "$mode" == "GOOD" ]]; then
    pmc_set_gm_np \
      clockClass "$GOOD_CLOCKCLASS" \
      clockAccuracy "$GOOD_CLOCKACCURACY" \
      offsetScaledLogVariance "$GOOD_OSLV" \
      currentUtcOffsetValid "$GOOD_UTC_VALID" \
      timeTraceable "$GOOD_TIMETRACE" \
      frequencyTraceable "$GOOD_FREQTRACE" \
      timeSource "$GOOD_TIMESOURCE"
    log "Applied GOOD GM advertisement (clockClass=$GOOD_CLOCKCLASS traceable=1)"
  else
    pmc_set_gm_np \
      clockClass "$DEG_CLOCKCLASS" \
      clockAccuracy "$DEG_CLOCKACCURACY" \
      offsetScaledLogVariance "$DEG_OSLV" \
      currentUtcOffsetValid "$DEG_UTC_VALID" \
      timeTraceable "$DEG_TIMETRACE" \
      frequencyTraceable "$DEG_FREQTRACE" \
      timeSource "$DEG_TIMESOURCE"
    log "Applied DEGRADED GM advertisement (clockClass=$DEG_CLOCKCLASS traceable=0)"
  fi
}

wait_for_ready_window() {
  # Wait for DPLL OK + ts2phc OK for TS2PHC_OK_HOLD_SEC continuously.
  # If holdover is allowed (and action=degrade), allow ts2phc NOT_OK as "ready" but we’ll stay DEGRADED.
  local start_ok=0
  while true; do
    (( stop_requested )) && exit 0

    if dpll_ok; then
      if ts2phc_ok; then
        if (( start_ok == 0 )); then
          start_ok="$(date +%s)"
        fi
        local now; now="$(date +%s)"
        if (( now - start_ok >= TS2PHC_OK_HOLD_SEC )); then
          return 0
        fi
      else
        start_ok=0
        if [[ "$PTP4L_NOT_OK_ACTION" == "degrade" && "$PTP4L_ALLOW_HOLDOVER" == "1" ]]; then
          # Allow starting in degraded holdover mode even without ts2phc convergence
          return 0
        fi
      fi
    else
      start_ok=0
    fi

    sleep "$POLL_SEC"
  done
}

start_ptp4l() {
  setsid bash -c 'exec "$@"' _ "${PTP4L_CMD[@]}" &
  ptp4l_pgid=$!
  log "Started ptp4l (pgid=$ptp4l_pgid)"
}

stop_ptp4l() {
  if (( ptp4l_pgid != 0 )); then
    log "Stopping ptp4l (pgid=$ptp4l_pgid)"
    kill -TERM "-$ptp4l_pgid" 2>/dev/null || true
    sleep 0.2
    kill -KILL "-$ptp4l_pgid" 2>/dev/null || true
    ptp4l_pgid=0
  fi
}

main() {
  log "Starting. action=$PTP4L_NOT_OK_ACTION hold_ts2phc=${TS2PHC_OK_HOLD_SEC}s allow_holdover=$PTP4L_ALLOW_HOLDOVER"

  if [[ "$PTP4L_NOT_OK_ACTION" == "degrade" ]]; then
    wait_for_pmc_ready
  fi

  while true; do
    wait_for_ready_window

    (( stop_requested )) && exit 0

    start_ptp4l

    # If degrade mode, set initial quality
    local last_quality=""
    if [[ "$PTP4L_NOT_OK_ACTION" == "degrade" ]]; then
      last_quality="INIT"
    fi

    while (( ptp4l_pgid != 0 )) && kill -0 "$ptp4l_pgid" 2>/dev/null; do
      (( stop_requested )) && exit 0

      local ok_dpll=0 ok_ts=0
      dpll_ok && ok_dpll=1 || ok_dpll=0
      ts2phc_ok && ok_ts=1 || ok_ts=0

      if [[ "$PTP4L_NOT_OK_ACTION" == "stop" ]]; then
        # Stop ptp4l on any instability:
        # - DPLL intervention window
        # - or ts2phc not converged
        if (( ok_dpll == 0 )) || (( ok_ts == 0 )); then
          stop_ptp4l
          break
        fi
      else
        # degrade mode: keep ptp4l running but toggle advertised quality
        local want="DEGRADED"
        if (( ok_dpll == 1 )) && (( ok_ts == 1 )); then
          want="GOOD"
        fi
        if [[ "$PTP4L_ALLOW_HOLDOVER" == "1" ]] && (( ok_dpll == 1 )) && (( ok_ts == 0 )); then
          want="DEGRADED"
        fi
        if [[ "$want" != "$last_quality" ]]; then
          apply_quality "$want"
          last_quality="$want"
        fi
      fi

      sleep "$POLL_SEC"
    done

    # If ptp4l exited unexpectedly, loop back and try again
    sleep 1
  done
}

main "$@"

