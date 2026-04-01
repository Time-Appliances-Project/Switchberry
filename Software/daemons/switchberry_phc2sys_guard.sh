#!/usr/bin/env bash
set -euo pipefail

# switchberry_phc2sys_guard.sh
#
# Supervises phc2sys to sync system clock (CLOCK_REALTIME) from the CM4
# Ethernet NIC PHC (/dev/ptp0).
#
# Works in two modes based on PTP role:
#
#   GM+GPS:  Gate on ts2phc status file being OK.
#            ts2phc disciplines the PHC from GPS, so once ts2phc is converged,
#            the PHC has accurate time and we can sync the system clock.
#
#   CLIENT:  Gate on ptp4l client being locked (servo s2 with tight offsets).
#            ptp4l disciplines the PHC from the network GM.
#
# Writes status to /tmp/switchberry-phc2sys.status (OK/NOT_OK).

CONFIG="${CONFIG:-/etc/startup-dpll.json}"

# Status files
TS2PHC_STATUS_FILE="${TS2PHC_STATUS_FILE:-/tmp/switchberry-ts2phc.status}"
PHC2SYS_STATUS_FILE="${PHC2SYS_STATUS_FILE:-/tmp/switchberry-phc2sys.status}"

POLL_SEC="${POLL_SEC:-1}"
RESTART_BACKOFF_SEC="${RESTART_BACKOFF_SEC:-2}"

# CLIENT mode: ptp4l lock quality checks
CLIENT_UNIT="${CLIENT_UNIT:-ptp4l-switchberry-client.service}"
JOURNAL_LINES="${JOURNAL_LINES:-200}"
OFFSET_SAMPLES="${OFFSET_SAMPLES:-5}"
OFFSET_ABS_MAX_NS="${OFFSET_ABS_MAX_NS:-1000}"

# phc2sys command: sync PHC -> system clock
# -s eth0: source is eth0 PHC
# -c CLOCK_REALTIME: destination is system clock
# -O -37: TAI-UTC offset (PHC runs in TAI, subtract 37 leap seconds to get UTC)
# -R 1: poll PHC once per second (reduce contention with ts2phc on genet driver)
# -m: log to stdout
# -q: no syslog
# -l 6: info level
PHC2SYS_CMD=(/usr/sbin/phc2sys -s eth0 -c CLOCK_REALTIME -O -37 -R 1 -m -q -l 6)

# Convergence criteria for phc2sys output
CONVERGE_S2_COUNT="${CONVERGE_S2_COUNT:-3}"
CONVERGE_OFFSET_NS="${CONVERGE_OFFSET_NS:-5000}"

ts() { date +"%Y-%m-%d %H:%M:%S"; }
iso() { date -Is; }
log() { echo "[$(ts)] phc2sys-guard: $*"; }

# --- Detect PTP role ---
detect_role() {
  python3 - <<EOF
import json, sys
try:
    cfg = json.load(open("${CONFIG}"))
    role = cfg.get("ptp_role", "NONE")
    gps = cfg.get("gps", {}).get("present", False)
    gps_role = cfg.get("gps", {}).get("role")
    print(f"{role} {gps} {gps_role}")
except Exception:
    print("NONE False None")
EOF
}

# --- Status file helpers ---
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

# --- GM mode: gate on ts2phc ---
ts2phc_ok() {
  [[ "$(read_first_line "$TS2PHC_STATUS_FILE")" == "OK" ]]
}

# --- CLIENT mode: gate on ptp4l lock quality ---
client_running() {
  systemctl is-active --quiet "$CLIENT_UNIT"
}

get_client_pid() {
  systemctl show --property MainPID --value "$CLIENT_UNIT"
}

current_servo_state() {
  local pid
  pid="$(get_client_pid)"
  if [[ -z "$pid" || "$pid" == "0" ]]; then return; fi
  journalctl _PID="$pid" -n "$JOURNAL_LINES" -o cat 2>/dev/null \
    | grep -E 'master offset' \
    | tail -n 1 \
    | sed -nE 's/.*master offset[[:space:]]+-?[0-9]+[[:space:]]+(s[0-9]+)[[:space:]]+freq.*/\1/p'
}

last_s2_offsets_ns() {
  local n="$1"
  local pid
  pid="$(get_client_pid)"
  if [[ -z "$pid" || "$pid" == "0" ]]; then return; fi
  journalctl _PID="$pid" -n "$JOURNAL_LINES" -o cat 2>/dev/null \
    | sed -nE 's/.*master offset[[:space:]]+(-?[0-9]+)[[:space:]]+s2[[:space:]]+freq.*/\1/p' \
    | tail -n "$n"
}

ptp4l_locked() {
  # Check: service running, servo s2, recent offsets tight
  if ! client_running; then return 1; fi

  local st
  st="$(current_servo_state)"
  if [[ "$st" != "s2" ]]; then return 1; fi

  local -a offs
  mapfile -t offs < <(last_s2_offsets_ns "$OFFSET_SAMPLES" || true)
  if (( ${#offs[@]} < OFFSET_SAMPLES )); then return 1; fi

  local s a
  for s in "${offs[@]}"; do
    a=$s; (( a < 0 )) && a=$(( -a ))
    if (( a > OFFSET_ABS_MAX_NS )); then return 1; fi
  done

  return 0
}

# --- Upstream OK check (mode-dependent) ---
MODE=""
upstream_ok() {
  if [[ "$MODE" == "GM" ]]; then
    ts2phc_ok
  elif [[ "$MODE" == "CLIENT" ]]; then
    ptp4l_locked
  else
    return 1
  fi
}

# --- Process management ---
phc2sys_pgid=0
fifo=""
reader_pid=0
stop_requested=0

cleanup() {
  stop_requested=1
  if (( phc2sys_pgid != 0 )); then
    log "Stopping phc2sys (pgid=$phc2sys_pgid)"
    kill -TERM "-$phc2sys_pgid" 2>/dev/null || true
    sleep 0.2
    kill -KILL "-$phc2sys_pgid" 2>/dev/null || true
  fi
  if (( reader_pid != 0 )); then
    kill -TERM "$reader_pid" 2>/dev/null || true
  fi
  [[ -n "$fifo" && -p "$fifo" ]] && rm -f "$fifo" || true
  write_status "$PHC2SYS_STATUS_FILE" "NOT_OK" "STOPPED"
  exit 0
}
trap cleanup TERM INT

start_phc2sys() {
  write_status "$PHC2SYS_STATUS_FILE" "NOT_OK" "STARTING"

  fifo="/tmp/phc2sys_guard.$$.fifo"
  rm -f "$fifo"
  mkfifo "$fifo"

  log "Starting phc2sys: ${PHC2SYS_CMD[*]}"
  setsid bash -c 'exec stdbuf -oL -eL "$@"' _ "${PHC2SYS_CMD[@]}" >"$fifo" 2>&1 &
  phc2sys_pgid=$!
  log "Started phc2sys (pgid=$phc2sys_pgid)"

  # Parse output for convergence
  local good_count=0

  (
    while IFS= read -r line; do
      echo "$line"

      # Parse lines like: "phc2sys[...]: eth0 offset  123 s2 freq -456"
      if [[ "$line" =~ [[:space:]]offset[[:space:]]+(-?[0-9]+)[[:space:]]+s([0-9]+)[[:space:]] ]]; then
        local offset="${BASH_REMATCH[1]}"
        local state="s${BASH_REMATCH[2]}"

        local abs_off="$offset"
        if (( abs_off < 0 )); then abs_off=$(( -abs_off )); fi

        if [[ "$state" == "s2" && "$abs_off" -le "$CONVERGE_OFFSET_NS" ]]; then
          good_count=$(( good_count + 1 ))
        else
          good_count=0
        fi

        if (( good_count >= CONVERGE_S2_COUNT )); then
          write_status "$PHC2SYS_STATUS_FILE" "OK" "CONVERGED state=$state offset_ns=$offset"
        else
          write_status "$PHC2SYS_STATUS_FILE" "NOT_OK" "NOT_CONVERGED state=$state offset_ns=$offset good=$good_count/$CONVERGE_S2_COUNT"
        fi
      fi
    done <"$fifo"
  ) &
  reader_pid=$!
}

stop_phc2sys() {
  if (( phc2sys_pgid != 0 )); then
    log "Stopping phc2sys (pgid=$phc2sys_pgid)"
    write_status "$PHC2SYS_STATUS_FILE" "NOT_OK" "UPSTREAM_LOST"
    kill -TERM "-$phc2sys_pgid" 2>/dev/null || true
    sleep 0.2
    kill -KILL "-$phc2sys_pgid" 2>/dev/null || true
    phc2sys_pgid=0
  fi
  if (( reader_pid != 0 )); then
    kill -TERM "$reader_pid" 2>/dev/null || true
    reader_pid=0
  fi
  [[ -n "$fifo" && -p "$fifo" ]] && rm -f "$fifo" || true
  fifo=""
}

# --- Main ---
ROLE_INFO="$(detect_role)"
read -r PTP_ROLE GPS_PRESENT GPS_ROLE <<< "$ROLE_INFO"

log "Detected: ptp_role=$PTP_ROLE gps_present=$GPS_PRESENT gps_role=$GPS_ROLE"

if [[ "$PTP_ROLE" == "GM" && "$GPS_PRESENT" == "True" && "$GPS_ROLE" != "None" ]]; then
  MODE="GM"
  log "Mode: GM+GPS — gating on ts2phc status"
elif [[ "$PTP_ROLE" == "CLIENT" ]]; then
  MODE="CLIENT"
  log "Mode: CLIENT — gating on ptp4l lock quality"
else
  log "PTP role is $PTP_ROLE (no GPS or NONE). phc2sys not needed. Exiting."
  write_status "$PHC2SYS_STATUS_FILE" "NOT_OK" "NOT_APPLICABLE"
  exit 0
fi

write_status "$PHC2SYS_STATUS_FILE" "NOT_OK" "INIT"

UPSTREAM_STABLE_COUNT="${UPSTREAM_STABLE_COUNT:-3}"

while true; do
  # Wait for upstream to be stably OK (multiple consecutive checks)
  stable_count=0
  while true; do
    (( stop_requested )) && exit 0
    if upstream_ok; then
      stable_count=$(( stable_count + 1 ))
      if (( stable_count >= UPSTREAM_STABLE_COUNT )); then
        log "Upstream stable for $UPSTREAM_STABLE_COUNT checks"
        break
      fi
    else
      if (( stable_count > 0 )); then
        log "Upstream flapped after $stable_count/$UPSTREAM_STABLE_COUNT checks, resetting"
      fi
      stable_count=0
    fi
    sleep "$POLL_SEC"
  done

  (( stop_requested )) && exit 0
  start_phc2sys

  # Monitor until upstream goes NOT_OK or phc2sys exits
  while true; do
    (( stop_requested )) && exit 0

    if ! upstream_ok; then
      log "Upstream lost — stopping phc2sys"
      stop_phc2sys
      break
    fi

    # If phc2sys died, break to restart
    if (( phc2sys_pgid != 0 )) && ! kill -0 "$phc2sys_pgid" 2>/dev/null; then
      write_status "$PHC2SYS_STATUS_FILE" "NOT_OK" "EXITED"
      phc2sys_pgid=0
      break
    fi

    sleep "$POLL_SEC"
  done

  sleep "$RESTART_BACKOFF_SEC"
done
