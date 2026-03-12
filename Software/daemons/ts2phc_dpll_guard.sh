#!/usr/bin/env bash
set -euo pipefail

# Input from ClockMatrix fastlock monitor
DPLL_STATUS_FILE="${DPLL_STATUS_FILE:-/tmp/switchberry-clockmatrix.status}"

# Output for other services to gate on
TS2PHC_STATUS_FILE="${TS2PHC_STATUS_FILE:-/tmp/switchberry-ts2phc.status}"

POLL_SEC="${POLL_SEC:-1}"
RESTART_BACKOFF_SEC="${RESTART_BACKOFF_SEC:-1}"

# Convergence criteria:
# - require CONVERGE_S2_COUNT consecutive "s2" samples
# - and abs(offset) <= CONVERGE_OFFSET_NS
CONVERGE_S2_COUNT="${CONVERGE_S2_COUNT:-5}"
CONVERGE_OFFSET_NS="${CONVERGE_OFFSET_NS:-1000000}" # 1 ms default

# Divergence detection:
# If ts2phc is in s2 but offset exceeds this threshold for DIVERGE_COUNT
# consecutive samples, restart ts2phc (the PHC is too far off to converge
# via frequency adjustment alone — needs a step reset).
DIVERGE_OFFSET_NS="${DIVERGE_OFFSET_NS:-1000000000}" # 1 second
DIVERGE_COUNT="${DIVERGE_COUNT:-10}"
DIVERGE_FLAG="/tmp/ts2phc_guard_diverged.$$"

TS2PHC_CMD_NMEA=(/usr/sbin/ts2phc -q -s nmea -c eth0 -f /etc/switchberry/ts2phc-switchberry.conf -m -l 7)
TS2PHC_CMD_GENERIC=(/usr/sbin/ts2phc -q -s generic -c eth0 -f /etc/switchberry/ts2phc-switchberry.conf -m -l 7)

# Drop noisy lines from stdout (still parsed internally)
FILTER_RE='nmea sentence|nmea delay'

ts() { date +"%Y-%m-%d %H:%M:%S"; }
iso() { date -Is; }
log() { echo "[$(ts)] ts2phc-guard: $*"; }

gps_ok() {
  local lines
  lines="$(gpspipe -r -x 2 2>/dev/null | wc -l)"
  (( lines > 10 ))
}


write_status() {
  local file="$1" state="$2" msg="$3"
  local tmp="${file}.$$".tmp
  printf "%s\n%s\n%s\n" "$state" "$(iso)" "$msg" > "$tmp"
  mv -f "$tmp" "$file"
}

read_first_line() {
  local file="$1"
  if [[ -f "$file" ]]; then
    head -n1 "$file" 2>/dev/null || echo "NOT_OK"
  else
    echo "NOT_OK"
  fi
}

dpll_ok() {
  [[ "$(read_first_line "$DPLL_STATUS_FILE")" == "OK" ]]
}

# Process bookkeeping
ts2phc_pgid=0
fifo=""
reader_pid=0

stop_requested=0

cleanup() {
  stop_requested=1
  if (( ts2phc_pgid != 0 )); then
    log "Stopping ts2phc (pgid=$ts2phc_pgid)"
    kill -TERM "-$ts2phc_pgid" 2>/dev/null || true
    sleep 0.2
    kill -KILL "-$ts2phc_pgid" 2>/dev/null || true
  fi
  if (( reader_pid != 0 )); then
    kill -TERM "$reader_pid" 2>/dev/null || true
  fi
  [[ -n "$fifo" && -p "$fifo" ]] && rm -f "$fifo" || true
  write_status "$TS2PHC_STATUS_FILE" "NOT_OK" "STOPPED"
  exit 0
}
trap cleanup TERM INT

start_ts2phc() {
  write_status "$TS2PHC_STATUS_FILE" "NOT_OK" "STARTING"

  fifo="/tmp/ts2phc_guard.$$.fifo"
  rm -f "$fifo"
  mkfifo "$fifo"

  # Start ts2phc in its own process group so we can kill it cleanly.
  # setsid makes PID == PGID for the launched process.


  local -a TS2PHC_CMD

  if gps_ok; then
    TS2PHC_CMD=("${TS2PHC_CMD_NMEA[@]}")
    log "GPS OK: starting ts2phc in NMEA mode: ${TS2PHC_CMD[*]}"
  else
    # copy system time to eth0 first
    sudo phc_ctl eth0 "set;" adj 37
    TS2PHC_CMD=("${TS2PHC_CMD_GENERIC[@]}")
    log "GPS not OK: setting eth0 PHC from system time, then starting ts2phc in GENERIC mode: ${TS2PHC_CMD[*]}"
  fi

  # Start ts2phc detached, line-buffered into the fifo
  setsid bash -c 'exec stdbuf -oL -eL "$@"' _ "${TS2PHC_CMD[@]}" >"$fifo" 2>&1 &


  ts2phc_pgid=$!
  log "Started ts2phc (pgid=$ts2phc_pgid)"

  # Parse output and update converged state
  local good_count=0
  local diverge_count=0
  local last_offset=0
  local last_state=""

  rm -f "$DIVERGE_FLAG"

  (
    while IFS= read -r line; do
      # Echo filtered logs (use bash builtin to avoid spawning grep per line)
      if ! [[ "$line" =~ $FILTER_RE ]]; then
        echo "$line"
      fi

      # Parse servo state lines like:
      #   "... /dev/ptp0 offset 5 s2 freq -4919"
      # s1=unlocked, s2=locked
      if [[ "$line" =~ [[:space:]]offset[[:space:]]+(-?[0-9]+)[[:space:]]+s([0-9]+)[[:space:]] ]]; then
        last_offset="${BASH_REMATCH[1]}"
        last_state="s${BASH_REMATCH[2]}"

        # abs(offset)
        local abs_off="$last_offset"
        if (( abs_off < 0 )); then abs_off=$(( -abs_off )); fi

        if [[ "$last_state" == "s2" && "$abs_off" -le "$CONVERGE_OFFSET_NS" ]]; then
          good_count=$(( good_count + 1 ))
          diverge_count=0
        else
          good_count=0
        fi

        # Divergence detection: s2 but offset is insanely large
        if [[ "$last_state" == "s2" ]] && (( abs_off > DIVERGE_OFFSET_NS )); then
          diverge_count=$(( diverge_count + 1 ))
          if (( diverge_count >= DIVERGE_COUNT )); then
            echo "[$(date +"%Y-%m-%d %H:%M:%S")] ts2phc-guard: DIVERGED — offset ${last_offset} ns exceeds ${DIVERGE_OFFSET_NS} ns for ${DIVERGE_COUNT} samples, requesting restart"
            write_status "$TS2PHC_STATUS_FILE" "NOT_OK" "DIVERGED offset_ns=$last_offset (>${DIVERGE_OFFSET_NS} for ${DIVERGE_COUNT} samples)"
            touch "$DIVERGE_FLAG"
            diverge_count=0
          fi
        else
          diverge_count=0
        fi

        if (( good_count >= CONVERGE_S2_COUNT )); then
          write_status "$TS2PHC_STATUS_FILE" "OK" "CONVERGED state=$last_state offset_ns=$last_offset"
        elif [[ ! -f "$DIVERGE_FLAG" ]]; then
          write_status "$TS2PHC_STATUS_FILE" "NOT_OK" "NOT_CONVERGED state=$last_state offset_ns=$last_offset good_count=$good_count/$CONVERGE_S2_COUNT"
        fi
      fi
    done <"$fifo"
  ) &
  reader_pid=$!
}

stop_ts2phc() {
  if (( ts2phc_pgid != 0 )); then
    log "Stopping ts2phc due to DPLL intervention (pgid=$ts2phc_pgid)"
    write_status "$TS2PHC_STATUS_FILE" "NOT_OK" "DPLL_INTERVENING"
    kill -TERM "-$ts2phc_pgid" 2>/dev/null || true
    sleep 0.2
    kill -KILL "-$ts2phc_pgid" 2>/dev/null || true
    ts2phc_pgid=0
  fi
  if (( reader_pid != 0 )); then
    kill -TERM "$reader_pid" 2>/dev/null || true
    reader_pid=0
  fi
  [[ -n "$fifo" && -p "$fifo" ]] && rm -f "$fifo" || true
  fifo=""
}

wait_for_dpll_ok() {
  while true; do
    dpll_ok && return 0
    sleep "$POLL_SEC"
  done
}

# Main loop
write_status "$TS2PHC_STATUS_FILE" "NOT_OK" "INIT"
while true; do
  wait_for_dpll_ok
  (( stop_requested )) && exit 0

  start_ts2phc

  # Monitor until DPLL goes NOT_OK, ts2phc exits, or divergence detected
  while true; do
    (( stop_requested )) && exit 0

    if ! dpll_ok; then
      stop_ts2phc
      break
    fi

    # If divergence detected (offset too large to converge), restart with step reset
    if [[ -f "$DIVERGE_FLAG" ]]; then
      log "Divergence detected — restarting ts2phc with PHC step-reset"
      stop_ts2phc
      rm -f "$DIVERGE_FLAG"
      # Step-reset the PHC to system time so ts2phc can converge from a sane starting point
      log "Step-resetting PHC to system time"
      sudo phc_ctl eth0 "set;" adj 37 2>/dev/null || true
      sleep 1
      break
    fi

    # If ts2phc died, break to restart later
    if (( ts2phc_pgid != 0 )) && ! kill -0 "$ts2phc_pgid" 2>/dev/null; then
      write_status "$TS2PHC_STATUS_FILE" "NOT_OK" "EXITED"
      ts2phc_pgid=0
      break
    fi

    sleep "$POLL_SEC"
  done

  sleep "$RESTART_BACKOFF_SEC"
done

