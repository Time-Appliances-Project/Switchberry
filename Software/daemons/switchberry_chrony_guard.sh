#!/usr/bin/env bash
set -euo pipefail

# switchberry_chrony_guard.sh
#
# Supervises chrony as an NTP server using the CM4 Ethernet NIC PHC
# (/dev/ptp0) as a refclock. Only runs in GM+GPS mode.
#
# Gating: ts2phc status file must be OK (PHC has accurate GPS time).
#
# When ts2phc goes NOT_OK (GPS lost, DPLL intervention), chrony is stopped
# so we don't serve stale time. When ts2phc recovers, chrony restarts.
#
# Writes status to /tmp/switchberry-chrony.status (OK/NOT_OK).

CONFIG="${CONFIG:-/etc/startup-dpll.json}"

# Status files
TS2PHC_STATUS_FILE="${TS2PHC_STATUS_FILE:-/tmp/switchberry-ts2phc.status}"
CHRONY_STATUS_FILE="${CHRONY_STATUS_FILE:-/tmp/switchberry-chrony.status}"

POLL_SEC="${POLL_SEC:-2}"
RESTART_BACKOFF_SEC="${RESTART_BACKOFF_SEC:-3}"

# Chrony config and runtime
CHRONY_CONF="${CHRONY_CONF:-/etc/switchberry/chrony-switchberry.conf}"
CHRONY_CMD=(/usr/sbin/chronyd -d -f "$CHRONY_CONF")

# How long ts2phc must be OK before starting chrony
TS2PHC_OK_HOLD_SEC="${TS2PHC_OK_HOLD_SEC:-5}"

ts() { date +"%Y-%m-%d %H:%M:%S"; }
iso() { date -Is; }
log() { echo "[$(ts)] chrony-guard: $*"; }

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

ts2phc_ok() {
  [[ "$(read_first_line "$TS2PHC_STATUS_FILE")" == "OK" ]]
}

# --- Process management ---
chrony_pgid=0
stop_requested=0

cleanup() {
  stop_requested=1
  stop_chrony
  write_status "$CHRONY_STATUS_FILE" "NOT_OK" "STOPPED"
  exit 0
}
trap cleanup TERM INT

start_chrony() {
  write_status "$CHRONY_STATUS_FILE" "NOT_OK" "STARTING"

  # Disable the stock chronyd if running (avoid port conflicts)
  systemctl stop chronyd.service 2>/dev/null || true
  systemctl stop chrony.service 2>/dev/null || true

  log "Starting chrony: ${CHRONY_CMD[*]}"
  setsid bash -c 'exec "$@"' _ "${CHRONY_CMD[@]}" &
  chrony_pgid=$!
  log "Started chrony (pgid=$chrony_pgid)"

  # Give chrony a moment to initialize, then mark OK
  sleep 2
  if kill -0 "$chrony_pgid" 2>/dev/null; then
    write_status "$CHRONY_STATUS_FILE" "OK" "SERVING"
    log "Chrony is serving NTP via PHC refclock"
  else
    write_status "$CHRONY_STATUS_FILE" "NOT_OK" "FAILED_TO_START"
    log "Chrony failed to start"
    chrony_pgid=0
  fi
}

stop_chrony() {
  if (( chrony_pgid != 0 )); then
    log "Stopping chrony (pgid=$chrony_pgid)"
    kill -TERM "-$chrony_pgid" 2>/dev/null || true
    sleep 0.5
    kill -KILL "-$chrony_pgid" 2>/dev/null || true
    chrony_pgid=0
  fi
  write_status "$CHRONY_STATUS_FILE" "NOT_OK" "UPSTREAM_LOST"
}

wait_for_ts2phc_stable() {
  local start_ok=0
  while true; do
    (( stop_requested )) && exit 0

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
    fi

    sleep "$POLL_SEC"
  done
}

# --- Main ---
ROLE_INFO="$(detect_role)"
read -r PTP_ROLE GPS_PRESENT GPS_ROLE <<< "$ROLE_INFO"

log "Detected: ptp_role=$PTP_ROLE gps_present=$GPS_PRESENT gps_role=$GPS_ROLE"

if [[ "$PTP_ROLE" != "GM" || "$GPS_PRESENT" != "True" || "$GPS_ROLE" == "None" ]]; then
  log "Not GM+GPS mode (role=$PTP_ROLE gps=$GPS_PRESENT). NTP server not needed. Exiting."
  write_status "$CHRONY_STATUS_FILE" "NOT_OK" "NOT_APPLICABLE"
  exit 0
fi

# Sanity: check chrony is installed
if ! command -v chronyd >/dev/null 2>&1; then
  log "ERROR: chronyd not found. Install chrony: sudo apt install chrony"
  write_status "$CHRONY_STATUS_FILE" "NOT_OK" "CHRONY_NOT_INSTALLED"
  exit 1
fi

if [[ ! -f "$CHRONY_CONF" ]]; then
  log "ERROR: Chrony config not found: $CHRONY_CONF"
  write_status "$CHRONY_STATUS_FILE" "NOT_OK" "NO_CONFIG"
  exit 1
fi

write_status "$CHRONY_STATUS_FILE" "NOT_OK" "INIT"

while true; do
  log "Waiting for ts2phc to be stable for ${TS2PHC_OK_HOLD_SEC}s..."
  wait_for_ts2phc_stable

  (( stop_requested )) && exit 0
  start_chrony

  # Monitor until ts2phc goes NOT_OK or chrony exits
  while true; do
    (( stop_requested )) && exit 0

    if ! ts2phc_ok; then
      log "ts2phc lost — stopping chrony NTP server"
      stop_chrony
      break
    fi

    # If chrony died, break to restart
    if (( chrony_pgid != 0 )) && ! kill -0 "$chrony_pgid" 2>/dev/null; then
      write_status "$CHRONY_STATUS_FILE" "NOT_OK" "EXITED"
      chrony_pgid=0
      break
    fi

    sleep "$POLL_SEC"
  done

  sleep "$RESTART_BACKOFF_SEC"
done
