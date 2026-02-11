#!/usr/bin/env bash
set -euo pipefail

# ------------------------------------------------------------------------------
# fastlock_1pps_fix.sh — ClockMatrix 8A34004 DPLL Monitor & Automatic Relocker
# ------------------------------------------------------------------------------
#
# OVERVIEW
# --------
# The Renesas 8A34004 DPLL can occasionally get "stuck" in non-optimal lock
# states (LOCKREC, LOCKACQ, or flapping between states) after input
# disturbances, power-up transients, or combo-bus frequency steps.  The
# hardware's built-in lock detector can also declare LOCKED while the actual
# phase offset is well above the configured lock threshold (observed at
# 300-400ns when the threshold is 100ns).
#
# This script runs as a systemd daemon (switchberry-dpll-monitor.service) and
# continuously monitors the DPLL channels.  When it detects a problem, it
# intervenes by cycling the channel FREERUN → NORMAL to force the DPLL to
# re-acquire lock from scratch.
#
#
# MONITORED CHANNELS
# ------------------
#   Channel 5 (FREQ_CH)   — Frequency DPLL, locks to SyncE / 10MHz sources
#   Channel 6 (GPS_CHANS) — Time/1PPS DPLL, locks to GPS / PTP 1PPS sources
#
#
# DECISION TRIGGERS (in order of evaluation per poll cycle)
# ---------------------------------------------------------
#
#   Decision 1: STATE FLAPPING
#     If a channel's state changes ≥ N times within a sliding window
#     (e.g. 6 changes in 5 seconds), it's flapping and unlikely to settle.
#     → Force relock pulse.
#
#   Decision 2: STUCK UNLOCKED
#     If a channel stays unlocked for longer than a threshold (e.g. 10-20s)
#     without the state being explainable as "no reference present" (i.e.
#     steady FREERUN with no state changes), it's stuck.
#     → Force relock pulse.
#
#   Decision 3: PHASE OFFSET TOO LARGE WITH HYSTERESIS
#     If a channel reports LOCKED but `dplltool get-phase` shows the absolute
#     phase offset exceeds a trigger threshold (default 250ns) consistently for
#     a sustained period (default 20s), the hardware lock detector is wrong.
#     → Force relock pulse.
#     Hysteresis: the exceed timer only clears when phase drops below a lower
#     clear threshold (default 150ns).  Phase values in the dead zone between
#     clear and trigger thresholds keep the timer running.
#     This catches false-lock conditions observed on Channel 6 where the DPLL
#     declares LOCKED at ~380ns offset instead of <100ns.
#
#   Note: Only Channel 6 (GPS_CHANS) is subject to relock decisions.
#   Channel 5 (FREQ_CH) is observed for stable-lock edge detection (which can
#   trigger GPS channel resets via combo-bus) but is never force-relocked.
#
#   Additionally, when Channel 5 first achieves stable lock, this script can
#   optionally force-reset Channel 6 (GPS_CHANS), since the combo-bus
#   frequency step from Ch5 locking may have disturbed Ch6's lock.
#
#
# RELOCK PULSE MECHANISM
# ----------------------
#   1. Set channel operating state to FREERUN (via dplltool set_oper_state)
#   2. Clear state-change sticky bit
#   3. Wait a short pulse duration (200-400ms)
#   4. Set channel operating state back to NORMAL
#   5. Clear sticky bit again
#   This forces the DPLL to restart lock acquisition from scratch.
#
#
# STATUS FILE
# -----------
# Writes a 3-line status file (default: /tmp/switchberry-clockmatrix.status):
#   Line 1: OK | NOT_OK
#   Line 2: ISO-8601 timestamp
#   Line 3: Short human-readable message
#
# Semantics:
#   OK     → The system is in a valid steady state (LOCKED, FREERUN, no-ref,
#            etc.) and this script is NOT actively intervening.
#   NOT_OK → This script is actively intervening or just intervened.
#            Downstream consumers (ts2phc, ptp4l) should pause.
#
# Other programs can simply check line 1 to gate their operation.
#
#
# CONFIGURATION KNOBS
# -------------------
# All tunable parameters are defined as shell variables at the top of the
# script.  The most important ones:
#
#   POLL_SEC                   — Main loop poll interval (default 0.2s)
#   STARTUP_AGGRESSIVE_SEC     — Use tighter thresholds for this many seconds
#                                after script start (default 60s)
#   FREQ_CH / GPS_CHANS        — Which DPLL channels to monitor
#   *_UNLOCK_SEC               — How long a channel can stay unlocked before
#                                triggering a relock
#   *_FLAP_COUNT / *_FLAP_WINDOW_SEC — Flapping detection sensitivity
#   *_RESET_COOLDOWN_SEC       — Minimum time between relock pulses
#   PHASE_THRESHOLD_SEC        — Phase offset trigger threshold for Decision 3
#   PHASE_CLEAR_THRESHOLD_SEC  — Phase must drop below this to clear the timer
#                                (hysteresis band between clear and trigger)
#   PHASE_EXCEED_SEC           — How long phase must exceed trigger threshold
#   PHASE_CHECK_INTERVAL_SEC   — How often to poll get-phase (avoid SPI spam)
#   FREQ_LOCK_EVENT_GPS_ACTION — What to do when Ch5 stably locks:
#                                0=nothing, 1=force reset GPS, 2=boost GPS monitoring
#
# Default status path: /tmp/switchberry-clockmatrix.status
# ------------------------------------------------------------------------------

# ----------------------------- Configuration ----------------------------------

POLL_SEC=0.2
STARTUP_AGGRESSIVE_SEC=60

FREQ_CH=5
GPS_CHANS=(6)

# --- Normal thresholds ---
FREQ_CH_UNLOCK_SEC=10
FREQ_CH_FLAP_WINDOW_SEC=5
FREQ_CH_FLAP_COUNT=6
FREQ_CH_RESET_COOLDOWN_SEC=12
FREQ_CH_RESET_PULSE_SEC=0.4

GPS_UNLOCK_SEC=20
GPS_FLAP_WINDOW_SEC=8
GPS_FLAP_COUNT=6
GPS_RESET_COOLDOWN_SEC=15
GPS_RESET_PULSE_SEC=0.25

# "No ref present" accept logic (unlocked + no state changes for a while is valid)
FREQ_CH_NOCHANGE_ACCEPT_SEC=10
GPS_NOCHANGE_ACCEPT_SEC=10

# Stable-lock definition for FREQ_CH (used for FREQ_CH->LOCKED event)
FREQ_CH_STABLE_LOCK_SEC=2

# --- Phase offset relock thresholds ---
# If a channel reports LOCKED but get-phase exceeds this threshold consistently,
# force a relock. This catches "false lock" where the DPLL hardware thinks it's
# locked but the phase offset is too large.
PHASE_THRESHOLD_SEC=250e-9       # 250 nanoseconds — trigger level
PHASE_CLEAR_THRESHOLD_SEC=150e-9 # 150 nanoseconds — must drop below this to clear timer (hysteresis)
PHASE_EXCEED_SEC=20              # must exceed threshold for this many seconds
PHASE_CHECK_INTERVAL_SEC=2       # how often to check phase (avoid spamming SPI)
PHASE_RELOCK_COOLDOWN_SEC=15     # cooldown after a phase-triggered relock

# --- Startup aggressive thresholds ---
FREQ_CH_UNLOCK_SEC_AGG=4
FREQ_CH_FLAP_WINDOW_SEC_AGG=4
FREQ_CH_FLAP_COUNT_AGG=4

GPS_UNLOCK_SEC_AGG=10
GPS_FLAP_WINDOW_SEC_AGG=6
GPS_FLAP_COUNT_AGG=4

# What to do when FREQ_CH becomes stably locked:
#   0 = do nothing special
#   1 = force reset GPS_CHANS (Ch6) immediately
#   2 = boost monitoring aggressiveness for GPS_CHANS
FREQ_LOCK_EVENT_GPS_ACTION=1
GPS_BOOST_SEC=30

# Optional gating: only supervise GPS channels when FREQ_CH is LOCKED
GATE_GPS_ON_FREQ_LOCKED=0

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

# FREQ_CH stable-lock edge detection
freq_locked_since_s=0
freq_stably_locked=0

# GPS temporary boost after FREQ_CH locks
gps_boost_until_s=0

# Phase offset tracking (per channel)
declare -A phase_exceed_since_s   # when phase first exceeded threshold (0 = not exceeding)
declare -A last_phase_check_s     # last time we checked phase
declare -A last_phase_reset_s     # last time we did a phase-triggered relock

init_ch() {
  local ch="$1" t; t="$(now_s)"
  last_seen_s["$ch"]="$t"
  last_change_s["$ch"]="$t"
  flap_win_start_s["$ch"]="$t"
  flap_count["$ch"]=0
  unlock_since_s["$ch"]=0
  last_reset_s["$ch"]=0
  phase_exceed_since_s["$ch"]=0
  last_phase_check_s["$ch"]=0
  last_phase_reset_s["$ch"]=0
  dpll_clear_statechg_sticky "$ch"
}

is_locked() { [[ "$1" == "LOCKED" ]]; }
is_lockrec() { [[ "$1" == "LOCKREC" ]]; }
is_lockacq() { [[ "$1" == "LOCKACQ" ]]; }

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
# If unlocked and hasn't had any state changes for N seconds, accept ONLY if the
# state indicates no reference is present (FREERUN, HOLDOVER).
# LOCKREC/LOCKACQ mean the DPLL sees a reference and is trying (but failing) to
# lock — those should NOT be accepted; the relock logic should handle them.
unlocked_is_acceptable_nochange() {
  local ch="$1" t="$2" st="$3" accept_sec="$4"
  if is_locked "$st"; then
    return 1
  fi

  local lc="${last_change_s[$ch]}"

  # Only consider "acceptable no-ref" once the state has been steady long enough.
  if (( t - lc < accept_sec )); then
    return 1
  fi

  # LOCKREC = lost lock, in recovery mode, won't trigger fastlock — just drifts.
  # LOCKACQ = acquiring lock, but can get stuck indefinitely.
  # Both should trigger a relock if stuck long enough.
  if is_lockrec "$st" || is_lockacq "$st"; then
    return 1
  fi

  # FREERUN / HOLDOVER with no state changes = genuinely no reference present
  return 0
}

update_freq_stable_lock_edge() {
  local t="$1" st_freq="$2"

  if is_locked "$st_freq"; then
    if (( freq_locked_since_s == 0 )); then
      freq_locked_since_s="$t"
    fi
    if (( freq_stably_locked == 0 )) && (( t - freq_locked_since_s >= FREQ_CH_STABLE_LOCK_SEC )); then
      freq_stably_locked=1
      log "EVENT: CH$FREQ_CH became STABLY LOCKED (>=${FREQ_CH_STABLE_LOCK_SEC}s). Likely combo-bus freq step."

      if (( FREQ_LOCK_EVENT_GPS_ACTION == 1 )); then
        log "EVENT-ACTION: Forcing GPS Channels reset due to CH$FREQ_CH stable lock"
        for ch in "${GPS_CHANS[@]}"; do
          force_reset_pulse "$ch" "$GPS_RESET_PULSE_SEC"
        done
      elif (( FREQ_LOCK_EVENT_GPS_ACTION == 2 )); then
        gps_boost_until_s=$(( t + GPS_BOOST_SEC ))
        log "EVENT-ACTION: Boost monitoring aggressiveness for GPS Chans for ${GPS_BOOST_SEC}s"
      else
        log "EVENT-ACTION: No special action configured (FREQ_LOCK_EVENT_GPS_ACTION=0)"
      fi
    fi
  else
    freq_locked_since_s=0
    if (( freq_stably_locked == 1 )); then
      log "EVENT: CH$FREQ_CH left LOCKED state (state=$st_freq)"
    fi
    freq_stably_locked=0
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
  if [[ "$ch" -eq "$FREQ_CH" ]]; then
    if (( aggressive )); then
      unlock_sec="$FREQ_CH_UNLOCK_SEC_AGG"
      flap_win="$FREQ_CH_FLAP_WINDOW_SEC_AGG"
      flap_cnt="$FREQ_CH_FLAP_COUNT_AGG"
    else
      unlock_sec="$FREQ_CH_UNLOCK_SEC"
      flap_win="$FREQ_CH_FLAP_WINDOW_SEC"
      flap_cnt="$FREQ_CH_FLAP_COUNT"
    fi
    cooldown="$FREQ_CH_RESET_COOLDOWN_SEC"
    pulse="$FREQ_CH_RESET_PULSE_SEC"
    nochange_accept="$FREQ_CH_NOCHANGE_ACCEPT_SEC"
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

  # Decision 3: Phase offset too large for too long
  # Runs during LOCKED, LOCKREC, and LOCKACQ — the phase monitor returns valid
  # data in all these states. This catches the LOCKED↔LOCKREC bouncing pattern
  # where the DPLL can't settle and phase keeps drifting.
  if is_locked "$st" || is_lockrec "$st" || is_lockacq "$st"; then
    local lpc="${last_phase_check_s[$ch]:-0}"
    if (( t - lpc >= PHASE_CHECK_INTERVAL_SEC )); then
      last_phase_check_s["$ch"]="$t"
      local phase_str; phase_str="$(dplltool get-phase "$ch" 2>/dev/null || echo "0")"
      # phase_str is in seconds (e.g. 3.882000000000e-07)
      # Compare absolute value against threshold using awk
      local exceeds; exceeds="$(awk -v p="$phase_str" -v thr="$PHASE_THRESHOLD_SEC" \
        'BEGIN { p_abs = (p < 0) ? -p : p; print (p_abs > thr) ? 1 : 0 }')"

      if (( exceeds == 1 )); then
        if (( phase_exceed_since_s["$ch"] == 0 )); then
          phase_exceed_since_s["$ch"]="$t"
          log "INFO: CH$ch state=$st phase=${phase_str}s exceeds threshold (${PHASE_THRESHOLD_SEC}s)"
        fi
        local pes="${phase_exceed_since_s[$ch]}"
        local exceed_dur=$(( t - pes ))
        if (( exceed_dur >= PHASE_EXCEED_SEC )); then
          local lpr="${last_phase_reset_s[$ch]:-0}"
          if (( t - lpr >= PHASE_RELOCK_COOLDOWN_SEC )); then
            log "DECIDE: CH$ch state=$st phase=${phase_str}s > ${PHASE_THRESHOLD_SEC}s for ${exceed_dur}s -> relock"
            last_phase_reset_s["$ch"]="$t"
            force_reset_pulse "$ch" "$pulse"
            phase_exceed_since_s["$ch"]=0
          else
            log "DECIDE: CH$ch phase relock needed but cooldown active"
          fi
        fi
      else
        # Phase is below trigger threshold. Apply hysteresis:
        # only clear the timer if phase drops below the CLEAR threshold.
        if (( phase_exceed_since_s["$ch"] > 0 )); then
          local below_clear; below_clear="$(awk -v p="$phase_str" -v thr="$PHASE_CLEAR_THRESHOLD_SEC" \
            'BEGIN { p_abs = (p < 0) ? -p : p; print (p_abs < thr) ? 1 : 0 }')"
          if (( below_clear == 1 )); then
            log "INFO: CH$ch phase=${phase_str}s dropped below clear threshold (${PHASE_CLEAR_THRESHOLD_SEC}s) — timer cleared"
            phase_exceed_since_s["$ch"]=0
          else
            local pes="${phase_exceed_since_s[$ch]}"
            local exceed_dur=$(( t - pes ))
            log "INFO: CH$ch phase=${phase_str}s in dead zone (${PHASE_CLEAR_THRESHOLD_SEC}..${PHASE_THRESHOLD_SEC}s), timer still running (${exceed_dur}s/${PHASE_EXCEED_SEC}s)"
          fi
        fi
      fi
    fi
  else
    # FREERUN / HOLDOVER = no reference, phase reading is meaningless
    phase_exceed_since_s["$ch"]=0
  fi

  return 0
}

update_overall_status() {
  # New semantics:
  #   OK     => valid steady state (even if UNLOCKED / NO_REF / FREERUN) as long as
  #            we are not currently/just intervening.
  #   NOT_OK => we are intervening (or just intervened within holdoff window).
  local t="$1"
  
  local st_freq
  st_freq="$(dpll_get_state "$FREQ_CH")"
  
  local gps_status_str=""
  local all_gps_locked=1
  local all_gps_no_ref=1

  for ch in "${GPS_CHANS[@]}"; do
      local st
      st="$(dpll_get_state "$ch")"
      gps_status_str="$gps_status_str CH$ch=$st"

      if [[ "$st" != "LOCKED" ]]; then
          all_gps_locked=0
      fi

      if ! unlocked_is_acceptable_nochange "$ch" "$t" "$st" "$GPS_NOCHANGE_ACCEPT_SEC"; then
          all_gps_no_ref=0
      fi
  done

  # If we intervened recently, keep NOT_OK briefly so consumers can safely pause.
  if (( last_any_intervention_s > 0 )) && (( t - last_any_intervention_s < STATUS_INTERVENTION_HOLDOFF_SEC )); then
    set_status "NOT_OK" "INTERVENING age=${t-last_any_intervention_s}s FREQ(CH$FREQ_CH)=$st_freq $gps_status_str"
    return 0
  fi

  # Otherwise: ALWAYS OK (even if no refs / freerun).
  # Add a small hint message for humans.
  local hint=""

  if (( all_gps_locked == 1 )); then
    hint="LOCKED" 
  elif (( all_gps_no_ref == 1 )); then
    hint="NO_REF"
  else
    hint="UNLOCKED"
  fi

  set_status "OK" "$hint FREQ(CH$FREQ_CH)=$st_freq $gps_status_str"
  return 0
}

# --------------------------------- Main ---------------------------------------

main() {
  local start_t; start_t="$(now_s)"
  log "dpll-monitor starting (poll=${POLL_SEC}s, startup_aggressive=${STARTUP_AGGRESSIVE_SEC}s)"
  log "Config: FREQ_LOCK_EVENT_GPS_ACTION=$FREQ_LOCK_EVENT_GPS_ACTION (0=none,1=force reset,2=boost ${GPS_BOOST_SEC}s)"
  log "Status file: ${STATUS_FILE} (OK only unless intervening). Holdoff: ${STATUS_INTERVENTION_HOLDOFF_SEC}s"
  log "No-ref accept: FREQ(CH$FREQ_CH)=${FREQ_CH_NOCHANGE_ACCEPT_SEC}s, GPS=${GPS_NOCHANGE_ACCEPT_SEC}s"

  set_status "OK" "STARTING"

  init_ch "$FREQ_CH"
  for ch in "${GPS_CHANS[@]}"; do init_ch "$ch"; done

  while true; do
    local t; t="$(now_s)"

    # --- FREQ Channel: observe state only (no relock intervention) ---
    # We still read Ch5 state for edge detection (combo-bus → GPS reset)
    # and for status reporting, but we do NOT run monitor_channel on it.
    local st_freq; st_freq="$(dpll_get_state "$FREQ_CH")"
    update_freq_stable_lock_edge "$t" "$st_freq"

    # --- GPS channels ---
    if (( GATE_GPS_ON_FREQ_LOCKED == 1 )) && [[ "$st_freq" != "LOCKED" ]]; then
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
