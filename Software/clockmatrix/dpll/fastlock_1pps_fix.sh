#!/usr/bin/env bash
set -euo pipefail

# ------------------------------------------------------------------------------
# fastlock_1pps_fix.sh  (ClockMatrix 8A34004 monitor + relock workaround)
#
# Status file semantics (HIGH LEVEL):
#   - OK     => The system is in a valid steady state (LOCKED, UNLOCKED, NO_REF,
#              FREERUN, etc.) and this script is NOT actively intervening.
#   - NOT_OK => This script is actively intervening / has just intervened
#              (i.e., it had to cycle one or more channels FREERUN->NORMAL).
#
# This is intentionally simple so other programs can just check line 1:
#   if "NOT_OK" -> pause/stop consumers (ts2phc/ptp4l/etc)
#   else        -> proceed (even if there is no reference / freerunning)
#
# Status file (3 lines):
#   1) OK | NOT_OK
#   2) ISO-8601 timestamp
#   3) short message
#
# Default status path: /tmp/switchberry-clockmatrix.status
# ------------------------------------------------------------------------------

# ----------------------------- Configuration ----------------------------------

POLL_SEC=0.2
STARTUP_AGGRESSIVE_SEC=60

CH2=2
GPS_CHANS=(5 6)

# --- Normal thresholds ---
CH2_UNLOCK_SEC=8
CH2_FLAP_WINDOW_SEC=5
CH2_FLAP_COUNT=6
CH2_RESET_COOLDOWN_SEC=12
CH2_RESET_PULSE_SEC=0.4

GPS_UNLOCK_SEC=20
GPS_FLAP_WINDOW_SEC=8
GPS_FLAP_COUNT=6
GPS_RESET_COOLDOWN_SEC=15
GPS_RESET_PULSE_SEC=0.25

# "No ref present" accept logic (unlocked + no state changes for a while is valid)
CH2_NOCHANGE_ACCEPT_SEC=10
GPS_NOCHANGE_ACCEPT_SEC=10

# Stable-lock definition for CH2 (used for CH2->LOCKED event)
CH2_STABLE_LOCK_SEC=2

# --- Startup aggressive thresholds ---
CH2_UNLOCK_SEC_AGG=4
CH2_FLAP_WINDOW_SEC_AGG=4
CH2_FLAP_COUNT_AGG=4

GPS_UNLOCK_SEC_AGG=10
GPS_FLAP_WINDOW_SEC_AGG=6
GPS_FLAP_COUNT_AGG=4

# What to do when CH2 becomes stably locked:
#   0 = do nothing special
#   1 = force reset CH5/6 immediately
#   2 = boost monitoring aggressiveness for CH5/6 for GPS_BOOST_SEC
CH2_LOCK_EVENT_GPS_ACTION=1
GPS_BOOST_SEC=30

# Optional gating: only supervise GPS channels when CH2 is LOCKED
GATE_GPS_ON_CH2_LOCKED=0

# ------------------------------ Status file -----------------------------------

STATUS_FILE="${STATUS_FILE:-/tmp/switchberry-clockmatrix.status}"

# How long to hold NOT_OK after an intervention (gives downstream consumers time to react)
STATUS_INTERVENTION_HOLDOFF_SEC="${STATUS_INTERVENTION_HOLDOFF_SEC:-2}"

last_status_state=""
last_status_msg=""
last_any_intervention_s=0

write_status() {
  local state="$1"; shift
  local msg="$*"
  local iso; iso="$(date -Is)"
  local tmp="${STATUS_FILE}.$$".tmp
  printf "%s\n%s\n%s\n" "$state" "$iso" "$msg" > "$tmp"
  mv "$tmp" "$STATUS_FILE"
}

set_status() {
  local state="$1"; shift
  local msg="$*"
  if [[ "$state" != "$last_status_state" || "$msg" != "$last_status_msg" ]]; then
    write_status "$state" "$msg"
    last_status_state="$state"
    last_status_msg="$msg"
  fi
}

cleanup() {
  # If the monitor dies/stops, make that explicit to consumers.
  write_status "NOT_OK" "STOPPED"
}
trap cleanup EXIT

# ------------------------------ Logging ---------------------------------------

ts() { date +"%Y-%m-%d %H:%M:%S"; }
log() { echo "[$(ts)] $*"; }

# ------------------------- dplltool wrapper API -------------------------------
# Adjust if your dplltool CLI differs.

dpll_get_state() {
  local ch="$1"
  dplltool get_state "$ch"
}

dpll_get_statechg_sticky() {
  local ch="$1"
  dplltool get_statechg_sticky "$ch"
}

dpll_clear_statechg_sticky() {
  local ch="$1"
  dplltool clear_statechg_sticky "$ch" >/dev/null
}

dpll_set_oper_state() {
  local ch="$1" state="$2" # NORMAL|FREERUN|HOLDOVER
  dplltool set_oper_state "$ch" "$state" >/dev/null
}

# ------------------------------ Time helpers ----------------------------------

now_s() { date +%s; }
sleep_s() { sleep "$1"; }

# --------------------------- Per-channel tracking -----------------------------

declare -A last_seen_s
declare -A last_change_s
declare -A flap_win_start_s
declare -A flap_count
declare -A unlock_since_s
declare -A last_reset_s

# CH2 stable-lock edge detection
ch2_locked_since_s=0
ch2_stably_locked=0

# GPS temporary boost after CH2 locks
gps_boost_until_s=0

init_ch() {
  local ch="$1" t; t="$(now_s)"
  last_seen_s["$ch"]="$t"
  last_change_s["$ch"]="$t"
  flap_win_start_s["$ch"]="$t"
  flap_count["$ch"]=0
  unlock_since_s["$ch"]=0
  last_reset_s["$ch"]=0
  dpll_clear_statechg_sticky "$ch"
}

is_locked() { [[ "$1" == "LOCKED" ]]; }
is_lockrec() { [[ "$1" == "LOCKREC" ]]; }

in_startup_aggressive() {
  local t="$1" start_t="$2"
  (( t - start_t < STARTUP_AGGRESSIVE_SEC ))
}

cooldown_ok() {
  local ch="$1" t="$2" cooldown="$3"
  local lr="${last_reset_s[$ch]:-0}"
  (( t - lr >= cooldown ))
}

mark_intervention() {
  local t; t="$(now_s)"
  last_any_intervention_s="$t"
}

force_reset_pulse() {
  local ch="$1" pulse="$2"

  local t; t="$(now_s)"
  last_reset_s["$ch"]="$t"
  mark_intervention

  # Immediately mark NOT_OK so consumers can pause quickly
  set_status "NOT_OK" "INTERVENING reset_pulse CH$ch"

  log "ACTION: CH$ch reset pulse: FREERUN (${pulse}s) -> NORMAL"
  dpll_set_oper_state "$ch" "FREERUN"
  dpll_clear_statechg_sticky "$ch"
  sleep_s "$pulse"
  dpll_set_oper_state "$ch" "NORMAL"
  dpll_clear_statechg_sticky "$ch"

  # reset local counters to avoid immediate retrigger
  flap_win_start_s["$ch"]="$(now_s)"
  flap_count["$ch"]=0
  unlock_since_s["$ch"]=0
}

update_sticky_and_flap() {
  local ch="$1" t="$2" st="$3" win_sec="$4"
  local sticky; sticky="$(dpll_get_statechg_sticky "$ch" || echo 0)"
  if [[ "$sticky" == "1" ]]; then
    dpll_clear_statechg_sticky "$ch"
    last_change_s["$ch"]="$t"

    local ws="${flap_win_start_s[$ch]}"
    if (( t - ws > win_sec )); then
      flap_win_start_s["$ch"]="$t"
      flap_count["$ch"]=0
    fi
    flap_count["$ch"]=$(( flap_count["$ch"] + 1 ))
    log "INFO: CH$ch sticky=1 state=$st flap_count=${flap_count[$ch]}/${win_sec}s"
  fi
}

update_unlock_timer() {
  local ch="$1" t="$2" st="$3"
  if is_locked "$st"; then
    unlock_since_s["$ch"]=0
  else
    if (( unlock_since_s["$ch"] == 0 )); then
      unlock_since_s["$ch"]="$t"
      log "INFO: CH$ch became UNLOCKED (state=$st)"
    fi
  fi
}

# "No ref present" acceptance:
# If unlocked and hasn't had any state changes for N seconds, accept and do not reset.
unlocked_is_acceptable_nochange() {
  local ch="$1" t="$2" st="$3" accept_sec="$4"
  if is_locked "$st"; then
    return 1
  fi

  local lc="${last_change_s[$ch]}"

  # Only consider "acceptable no-ref" once the state has been steady long enough.
  # (Before that, we're still waiting to see if it recovers / stabilizes.)
  if (( t - lc < accept_sec )); then
	# If we've been stuck in LOCKREC for the full accept_sec window,
	# do NOT accept it as "no ref present" (allow the reset logic to run).
    if is_lockrec "$st"; then
      return 0
    fi
    return 1
  fi


  return 0
}

update_ch2_stable_lock_edge() {
  local t="$1" st2="$2"

  if is_locked "$st2"; then
    if (( ch2_locked_since_s == 0 )); then
      ch2_locked_since_s="$t"
    fi
    if (( ch2_stably_locked == 0 )) && (( t - ch2_locked_since_s >= CH2_STABLE_LOCK_SEC )); then
      ch2_stably_locked=1
      log "EVENT: CH2 became STABLY LOCKED (>=${CH2_STABLE_LOCK_SEC}s). Likely combo-bus freq step."

      if (( CH2_LOCK_EVENT_GPS_ACTION == 1 )); then
        log "EVENT-ACTION: Forcing CH${GPS_CHANS[0]}/CH${GPS_CHANS[1]} reset due to CH2 stable lock"
        for ch in "${GPS_CHANS[@]}"; do
          force_reset_pulse "$ch" "$GPS_RESET_PULSE_SEC"
        done
      elif (( CH2_LOCK_EVENT_GPS_ACTION == 2 )); then
        gps_boost_until_s=$(( t + GPS_BOOST_SEC ))
        log "EVENT-ACTION: Boost monitoring aggressiveness for CH5/6 for ${GPS_BOOST_SEC}s"
      else
        log "EVENT-ACTION: No special CH5/6 action configured (CH2_LOCK_EVENT_GPS_ACTION=0)"
      fi
    fi
  else
    ch2_locked_since_s=0
    if (( ch2_stably_locked == 1 )); then
      log "EVENT: CH2 left LOCKED state (state=$st2)"
    fi
    ch2_stably_locked=0
  fi
}

monitor_channel() {
  local ch="$1" t="$2" start_t="$3" forced_aggressive="$4"

  local st; st="$(dpll_get_state "$ch")"

  local aggressive=0
  if (( forced_aggressive == 1 )) || in_startup_aggressive "$t" "$start_t"; then
    aggressive=1
  fi

  local unlock_sec flap_win flap_cnt cooldown pulse nochange_accept
  if [[ "$ch" -eq "$CH2" ]]; then
    if (( aggressive )); then
      unlock_sec="$CH2_UNLOCK_SEC_AGG"
      flap_win="$CH2_FLAP_WINDOW_SEC_AGG"
      flap_cnt="$CH2_FLAP_COUNT_AGG"
    else
      unlock_sec="$CH2_UNLOCK_SEC"
      flap_win="$CH2_FLAP_WINDOW_SEC"
      flap_cnt="$CH2_FLAP_COUNT"
    fi
    cooldown="$CH2_RESET_COOLDOWN_SEC"
    pulse="$CH2_RESET_PULSE_SEC"
    nochange_accept="$CH2_NOCHANGE_ACCEPT_SEC"
  else
    if (( aggressive )); then
      unlock_sec="$GPS_UNLOCK_SEC_AGG"
      flap_win="$GPS_FLAP_WINDOW_SEC_AGG"
      flap_cnt="$GPS_FLAP_COUNT_AGG"
    else
      unlock_sec="$GPS_UNLOCK_SEC"
      flap_win="$GPS_FLAP_WINDOW_SEC"
      flap_cnt="$GPS_FLAP_COUNT"
    fi
    cooldown="$GPS_RESET_COOLDOWN_SEC"
    pulse="$GPS_RESET_PULSE_SEC"
    nochange_accept="$GPS_NOCHANGE_ACCEPT_SEC"
  fi

  update_sticky_and_flap "$ch" "$t" "$st" "$flap_win"
  update_unlock_timer "$ch" "$t" "$st"

  # Heartbeat every ~10s
  local ls="${last_seen_s[$ch]}"
  if (( t - ls >= 10 )); then
    local us="${unlock_since_s[$ch]}"
    local unlocked_for=0
    if (( us > 0 )); then unlocked_for=$(( t - us )); fi
    log "CHECK: CH$ch state=$st unlocked_for=${unlocked_for}s flap=${flap_count[$ch]} mode=$([[ $aggressive -eq 1 ]] && echo aggressive || echo normal)"
    last_seen_s["$ch"]="$t"
  fi

  # Decision 1: flapping -> reset
  if (( flap_count["$ch"] >= flap_cnt )); then
    if cooldown_ok "$ch" "$t" "$cooldown"; then
      log "DECIDE: CH$ch flapping (>=${flap_cnt} changes in ${flap_win}s) -> reset"
      force_reset_pulse "$ch" "$pulse"
    else
      log "DECIDE: CH$ch flapping but cooldown active -> no action"
    fi
    return 0
  fi

  # Decision 2: unlocked too long
  local us="${unlock_since_s[$ch]}"
  if (( us > 0 )) && (( t - us >= unlock_sec )); then

    # Accept steady unlocked state (no ref present): do NOT reset.
    if unlocked_is_acceptable_nochange "$ch" "$t" "$st" "$nochange_accept"; then
      log "DECIDE: CH$ch unlocked ${t-us}s but NO state changes for >=${nochange_accept}s -> accept (no ref), no reset"
      # damp repeated logs
      unlock_since_s["$ch"]="$t"
      flap_win_start_s["$ch"]="$t"
      flap_count["$ch"]=0
      return 0
    fi

    if cooldown_ok "$ch" "$t" "$cooldown"; then
      log "DECIDE: CH$ch unlocked ${t-us}s (>=${unlock_sec}s) -> reset"
      force_reset_pulse "$ch" "$pulse"
    else
      log "DECIDE: CH$ch unlocked too long but cooldown active -> no action"
    fi
  fi

  return 0
}

update_overall_status() {
  # New semantics:
  #   OK     => valid steady state (even if UNLOCKED / NO_REF / FREERUN) as long as
  #            we are not currently/just intervening.
  #   NOT_OK => we are intervening (or just intervened within holdoff window).
  local t="$1"

  local st2 st5 st6
  st2="$(dpll_get_state "$CH2")"
  st5="$(dpll_get_state "${GPS_CHANS[0]}")"
  st6="$(dpll_get_state "${GPS_CHANS[1]}")"

  # If we intervened recently, keep NOT_OK briefly so consumers can safely pause.
  if (( last_any_intervention_s > 0 )) && (( t - last_any_intervention_s < STATUS_INTERVENTION_HOLDOFF_SEC )); then
    set_status "NOT_OK" "INTERVENING age=${t-last_any_intervention_s}s CH2=$st2 CH5=$st5 CH6=$st6"
    return 0
  fi

  # Otherwise: ALWAYS OK (even if no refs / freerun).
  # Add a small hint message for humans.
  local hint=""

  if [[ "$st5" != "LOCKED" || "$st6" != "LOCKED" ]]; then
    local no_ref5=0 no_ref6=0
    if unlocked_is_acceptable_nochange "${GPS_CHANS[0]}" "$t" "$st5" "$GPS_NOCHANGE_ACCEPT_SEC"; then no_ref5=1; fi
    if unlocked_is_acceptable_nochange "${GPS_CHANS[1]}" "$t" "$st6" "$GPS_NOCHANGE_ACCEPT_SEC"; then no_ref6=1; fi

    if (( no_ref5 == 1 && no_ref6 == 1 )); then
      hint="NO_REF"
    else
      hint="UNLOCKED"
    fi
  else
    hint="LOCKED"
  fi

  set_status "OK" "$hint CH2=$st2 CH5=$st5 CH6=$st6"
  return 0
}

# --------------------------------- Main ---------------------------------------

main() {
  local start_t; start_t="$(now_s)"
  log "dpll-monitor starting (poll=${POLL_SEC}s, startup_aggressive=${STARTUP_AGGRESSIVE_SEC}s)"
  log "Config: CH2_LOCK_EVENT_GPS_ACTION=$CH2_LOCK_EVENT_GPS_ACTION (0=none,1=force reset,2=boost ${GPS_BOOST_SEC}s)"
  log "Status file: ${STATUS_FILE} (OK only unless intervening). Holdoff: ${STATUS_INTERVENTION_HOLDOFF_SEC}s"
  log "No-ref accept: CH2=${CH2_NOCHANGE_ACCEPT_SEC}s, CH5/6=${GPS_NOCHANGE_ACCEPT_SEC}s"

  set_status "OK" "STARTING"

  init_ch "$CH2"
  for ch in "${GPS_CHANS[@]}"; do init_ch "$ch"; done

  while true; do
    local t; t="$(now_s)"

    # --- Channel 2 first ---
    local st2; st2="$(dpll_get_state "$CH2")"
    update_ch2_stable_lock_edge "$t" "$st2"
    monitor_channel "$CH2" "$t" "$start_t" 0

    # --- GPS channels ---
    if (( GATE_GPS_ON_CH2_LOCKED == 1 )) && [[ "$st2" != "LOCKED" ]]; then
      update_overall_status "$t"
      sleep_s "$POLL_SEC"
      continue
    fi

    local gps_forced_aggressive=0
    if (( gps_boost_until_s > 0 )) && (( t < gps_boost_until_s )); then
      gps_forced_aggressive=1
    fi

    for ch in "${GPS_CHANS[@]}"; do
      monitor_channel "$ch" "$t" "$start_t" "$gps_forced_aggressive"
    done

    # --- Update high-level status for other services ---
    update_overall_status "$t"

    sleep_s "$POLL_SEC"
  done
}

main "$@"

