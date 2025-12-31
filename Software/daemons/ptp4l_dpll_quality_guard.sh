#!/usr/bin/env bash
set -euo pipefail

STATUS_FILE="${STATUS_FILE:-/tmp/switchberry-clockmatrix.status}"
POLL_SEC="${POLL_SEC:-0.2}"

# pmc over UDS. Default linuxptp socket path is commonly /var/run/ptp4l
PMC_SOCK="${PMC_SOCK:-/var/run/ptp4l}"
PMC_BASE=(pmc -u -b 0 -t 1 -s "$PMC_SOCK")

# -------------------------
# GOOD profile (defaults)
# -------------------------
GOOD_CLOCKCLASS="${GOOD_CLOCKCLASS:-6}"        # typical "primary reference" class
GOOD_CLOCKACCURACY="${GOOD_CLOCKACCURACY:-0x22}" # don't over-claim; tweak as desired
GOOD_OSLV="${GOOD_OSLV:-0x0001}"              # stability hint; tweak as desired
GOOD_UTC_VALID="${GOOD_UTC_VALID:-1}"
GOOD_TIMETRACE="${GOOD_TIMETRACE:-1}"
GOOD_FREQTRACE="${GOOD_FREQTRACE:-1}"
GOOD_TIMESOURCE="${GOOD_TIMESOURCE:-0x20}"    # gps (IEEE 1588 timeSource)

# -------------------------
# DEGRADED profile (defaults)
# -------------------------
DEG_CLOCKCLASS="${DEG_CLOCKCLASS:-248}"       # linuxptp default / "degraded" common practice
DEG_CLOCKACCURACY="${DEG_CLOCKACCURACY:-0xfe}"
DEG_OSLV="${DEG_OSLV:-0xffff}"
DEG_UTC_VALID="${DEG_UTC_VALID:-0}"
DEG_TIMETRACE="${DEG_TIMETRACE:-0}"
DEG_FREQTRACE="${DEG_FREQTRACE:-0}"
DEG_TIMESOURCE="${DEG_TIMESOURCE:-0xa0}"      # internal oscillator (common)

ts() { date +"%Y-%m-%d %H:%M:%S"; }
log() { echo "[$(ts)] ptp4l-quality-guard: $*"; }

read_status() {
  if [[ -f "$STATUS_FILE" ]]; then
    head -n1 "$STATUS_FILE" 2>/dev/null || echo "NOT_OK"
  else
    echo "NOT_OK"
  fi
}

wait_for_pmc_ready() {
  while true; do
    if [[ -S "$PMC_SOCK" || -e "$PMC_SOCK" ]]; then
      if "${PMC_BASE[@]}" "GET PORT_DATA_SET" >/dev/null 2>&1; then
        return 0
      fi
    fi
    sleep 0.2
  done
}

pmc_get_gm_settings() {
  # Grab key/value pairs from GRANDMASTER_SETTINGS_NP.
  # Output: "key value" per line.
  "${PMC_BASE[@]}" "GET GRANDMASTER_SETTINGS_NP" 2>/dev/null \
    | awk 'NF>=2 {print $1, $2}' \
    | grep -E '^(clockClass|clockAccuracy|offsetScaledLogVariance|currentUtcOffset|leap61|leap59|currentUtcOffsetValid|ptpTimescale|timeTraceable|frequencyTraceable|timeSource) '
}

# Build a " key val key val ..." string from "key val" lines
kv_from_lines() {
  local out=""
  while read -r k v; do
    out+=" $k $v"
  done
  echo "$out"
}

# Ensure key exists and set it to val in " key val ..." kv string
kv_set() {
  local kv="$1" key="$2" val="$3"

  if echo "$kv" | grep -Eq "(^| )${key} "; then
    # replace existing
    echo "$kv" | sed -E "s/(^| )(${key}) [^ ]+/\1\2 ${val}/"
  else
    # append if missing
    echo "$kv ${key} ${val}"
  fi
}

pmc_set_gm_settings_kv() {
  local kv="$1"
  # shellcheck disable=SC2086
  "${PMC_BASE[@]}" "SET GRANDMASTER_SETTINGS_NP${kv}" >/dev/null
}

apply_profile() {
  local profile="$1" t
  t="$(ts)"

  # Use current GM settings as a base so we preserve utc offset + leap flags + ptpTimescale.
  local base_lines base_kv
  base_lines="$(pmc_get_gm_settings || true)"
  base_kv="$(printf "%s\n" "$base_lines" | kv_from_lines)"

  # If GET fails (rare), fall back to a conservative base.
  # Keep currentUtcOffset at 37 as a placeholder; adjust if your deployment needs different.
  if [[ -z "$base_kv" ]]; then
    base_kv=" clockClass 248 clockAccuracy 0xfe offsetScaledLogVariance 0xffff currentUtcOffset 37 leap61 0 leap59 0 currentUtcOffsetValid 0 ptpTimescale 1 timeTraceable 0 frequencyTraceable 0 timeSource 0xa0"
  fi

  local kv="$base_kv"
  if [[ "$profile" == "GOOD" ]]; then
    kv="$(kv_set "$kv" clockClass "$GOOD_CLOCKCLASS")"
    kv="$(kv_set "$kv" clockAccuracy "$GOOD_CLOCKACCURACY")"
    kv="$(kv_set "$kv" offsetScaledLogVariance "$GOOD_OSLV")"
    kv="$(kv_set "$kv" currentUtcOffsetValid "$GOOD_UTC_VALID")"
    kv="$(kv_set "$kv" timeTraceable "$GOOD_TIMETRACE")"
    kv="$(kv_set "$kv" frequencyTraceable "$GOOD_FREQTRACE")"
    kv="$(kv_set "$kv" timeSource "$GOOD_TIMESOURCE")"
    pmc_set_gm_settings_kv "$kv"
    log "Applied GOOD GM settings (clockClass=$GOOD_CLOCKCLASS accuracy=$GOOD_CLOCKACCURACY traceable=1)"
  else
    kv="$(kv_set "$kv" clockClass "$DEG_CLOCKCLASS")"
    kv="$(kv_set "$kv" clockAccuracy "$DEG_CLOCKACCURACY")"
    kv="$(kv_set "$kv" offsetScaledLogVariance "$DEG_OSLV")"
    kv="$(kv_set "$kv" currentUtcOffsetValid "$DEG_UTC_VALID")"
    kv="$(kv_set "$kv" timeTraceable "$DEG_TIMETRACE")"
    kv="$(kv_set "$kv" frequencyTraceable "$DEG_FREQTRACE")"
    kv="$(kv_set "$kv" timeSource "$DEG_TIMESOURCE")"
    pmc_set_gm_settings_kv "$kv"
    log "Applied DEGRADED GM settings (clockClass=$DEG_CLOCKCLASS accuracy=$DEG_CLOCKACCURACY traceable=0)"
  fi
}

main() {
  log "Starting. STATUS_FILE=$STATUS_FILE PMC_SOCK=$PMC_SOCK POLL_SEC=$POLL_SEC"
  log "Profiles: GOOD(clockClass=$GOOD_CLOCKCLASS accuracy=$GOOD_CLOCKACCURACY) DEG(clockClass=$DEG_CLOCKCLASS accuracy=$DEG_CLOCKACCURACY)"

  wait_for_pmc_ready

  # Apply once at start based on current status
  local last=""
  while true; do
    local st; st="$(read_status)"
    local want="DEGRADED"
    [[ "$st" == "OK" ]] && want="GOOD"

    if [[ "$want" != "$last" ]]; then
      apply_profile "$want"
      last="$want"
    fi

    sleep "$POLL_SEC"
  done
}

main "$@"

