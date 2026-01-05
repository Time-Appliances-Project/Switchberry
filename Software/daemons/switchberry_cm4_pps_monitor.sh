#!/usr/bin/env bash
set -euo pipefail

# --- Config (override via Environment= in systemd unit if you want) ---
PTP_DEVICE="${PTP_DEVICE:-/dev/ptp0}"
CLIENT_UNIT="${CLIENT_UNIT:-ptp4l-switchberry-client.service}"

PPS_PERIOD_NS="${PPS_PERIOD_NS:-1000000000}"  # 1 Hz PPS
TESTPTP_BIN="${TESTPTP_BIN:-testptp}"

POLL_INTERVAL_SEC="${POLL_INTERVAL_SEC:-2}"
JOURNAL_LINES="${JOURNAL_LINES:-250}"

# Require N consecutive "locked" polls before enabling PPS (reduces flapping)
LOCK_STABLE_COUNT="${LOCK_STABLE_COUNT:-3}"

# If pmc is available, also require portState=SLAVE (more robust)
REQUIRE_PMC_SLAVE="${REQUIRE_PMC_SLAVE:-1}"

log() { echo "cm4-pps-monitor: $*"; }

disable_pps() {
  if command -v "$TESTPTP_BIN" >/dev/null 2>&1; then
    "$TESTPTP_BIN" -d "$PTP_DEVICE" -L 0,0 >/dev/null 2>&1 || true
  fi
}

enable_pps() {
  "$TESTPTP_BIN" -d "$PTP_DEVICE" -L 0,2
  "$TESTPTP_BIN" -d "$PTP_DEVICE" -p "$PPS_PERIOD_NS"
}

client_active() {
  systemctl is-active --quiet "$CLIENT_UNIT"
}

journal_since_client_start() {
  # Use service active-enter time to avoid matching old logs from previous runs
  local since
  since="$(systemctl show -p ActiveEnterTimestamp --value "$CLIENT_UNIT" 2>/dev/null || true)"
  if [[ -n "${since:-}" && "$since" != "n/a" ]]; then
    journalctl -u "$CLIENT_UNIT" --since "$since" -n "$JOURNAL_LINES" --no-pager -o cat 2>/dev/null || true
  else
    journalctl -u "$CLIENT_UNIT" -n "$JOURNAL_LINES" --no-pager -o cat 2>/dev/null || true
  fi
}

last_servo_state() {
  # Looks for the last occurrence of "servo state sX" in ptp4l logs.
  # Example line often contains: "servo state s2"
  local txt
  txt="$(journal_since_client_start)"
  echo "$txt" | grep -Eo 'servo state s[0-9]+' | tail -n1 | awk '{print $NF}' || true
}

pmc_port_is_slave() {
  # Optional: if pmc exists, ask ptp4l via UDS for PORT_DATA_SET and require SLAVE.
  # This is a best-effort check; if pmc fails, we return "unknown" (non-zero).
  command -v pmc >/dev/null 2>&1 || return 2

  # pmc -u uses ptp4l's local UDS management socket (common linuxptp setup)
  local out state
  out="$(pmc -u -b 0 -t 1 "GET PORT_DATA_SET" 2>/dev/null || true)"
  state="$(echo "$out" | awk '/portState/ {print $2; exit}')"

  [[ "$state" == "SLAVE" ]]
}

is_locked() {
  # Must be active
  client_active || return 1

  # Must have /dev/ptp* available
  [[ -e "$PTP_DEVICE" ]] || return 1

  # Must have testptp available
  command -v "$TESTPTP_BIN" >/dev/null 2>&1 || return 1

  # Must be servo s2
  local s
  s="$(last_servo_state)"
  [[ "$s" == "s2" ]] || return 1

  # Optional: also require portState=SLAVE if pmc is present/enabled
  if [[ "$REQUIRE_PMC_SLAVE" == "1" ]]; then
    if command -v pmc >/dev/null 2>&1; then
      pmc_port_is_slave || return 1
    fi
  fi

  return 0
}

# Ensure PPS is disabled on exit (or when service is stopped)
trap 'log "Exiting; disabling PPS"; disable_pps' EXIT

log "Starting. device=$PTP_DEVICE unit=$CLIENT_UNIT poll=${POLL_INTERVAL_SEC}s stable=$LOCK_STABLE_COUNT require_pmc_slave=$REQUIRE_PMC_SLAVE"

pps_enabled=0
stable_locked=0

# Start safe: disable PPS until we know we are locked
disable_pps

while true; do
  if is_locked; then
    stable_locked=$((stable_locked + 1))
    if [[ $pps_enabled -eq 0 && $stable_locked -ge $LOCK_STABLE_COUNT ]]; then
      log "ptp4l locked (servo s2); enabling PPS output via testptp"
      if enable_pps; then
        pps_enabled=1
        log "PPS enabled"
      else
        log "Failed to enable PPS; will retry"
        disable_pps
        pps_enabled=0
        stable_locked=0
      fi
    fi
  else
    stable_locked=0
    if [[ $pps_enabled -eq 1 ]]; then
      log "ptp4l not locked / not running; disabling PPS"
      disable_pps
      pps_enabled=0
    fi
  fi

  sleep "$POLL_INTERVAL_SEC"
done

