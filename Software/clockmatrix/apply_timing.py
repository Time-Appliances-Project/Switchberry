#!/usr/bin/env python3
import argparse
import subprocess
import math
from typing import Dict, Tuple

# Import your existing config logic
# Make sure config.py (the wizard/validate script) is in the same directory or PYTHONPATH.
from config import TimingConfig, load_config, derive_plan, PtpRole, TimeFreqRole

# ---------- Constants ----------
DCO_FREQ_HZ = 500_000_000  # 500 MHz nominal DCO frequency (default for 8A3400x)


# ---------- Utility for running shell commands ----------

def run(cmd, dry_run: bool = False):
    print("RUN:", " ".join(cmd))
    if not dry_run:
        subprocess.run(cmd, check=True)


# Helper: User (Rear) vs HW (Front) mapping for logging
def get_user_sma_name(hw_name: str) -> str:
    # HW SMA1 <-> User SMA4
    # HW SMA2 <-> User SMA3
    # HW SMA3 <-> User SMA2
    # HW SMA4 <-> User SMA1
    if hw_name == "SMA1": return "User SMA4"
    if hw_name == "SMA2": return "User SMA3"
    if hw_name == "SMA3": return "User SMA2"
    if hw_name == "SMA4": return "User SMA1"
    return hw_name



# ---------- Mapping: logical source name -> DPLL input index ----------


# ---------- Logical DPLL input index (1..4) -> physical index mapping ----------

def logical_to_physical_input_idx(logical_idx: int) -> int:
    """
    Map logical DPLL input indices (1..4) to physical DPLL input indices, as used
    by dplltool now.

      Logical 1 -> Physical 0
      Logical 2 -> Physical 8 (N of 0)
      Logical 3 -> Physical 1
      Logical 4 -> Physical 9 (N of 1)
    """
    mapping = {
        1: 0,
        2: 8,
        3: 1,
        4: 9,
    }
    try:
        return mapping[logical_idx]
    except KeyError:
        raise ValueError(f"Invalid logical DPLL input index {logical_idx}, "
                         f"expected 1..4")


def map_source_to_input_idx(source_name: str) -> int:
    """
    Map high-level source name from config/plan to a physical DPLL input index.

    Based on your board wiring:
      - DPLL Input 1 -> SyncE OR SMA1 input path
      - DPLL Input 2 -> SMA2 input path OR CM4 1PPS
      - DPLL Input 3 -> SMA3 input path OR GPS PPS path
      - DPLL Input 4 -> SMA4 (no mux)
    """
    if source_name == "GPS":
        return 3
    if source_name == "SYNC_E":
        return 1
    if source_name == "CM4":
        return 2
    if source_name == "SMA1":
        return 1
    if source_name == "SMA2":
        return 2
    if source_name == "SMA3":
        return 3
    if source_name == "SMA4":
        return 4

    raise ValueError(f"Unknown source name '{source_name}' in plan → input mapping.")


# ---------- Infer nominal input frequency for each source ----------

def nominal_input_freq_hz(source_name: str, role: TimeFreqRole) -> int:
    """
    Guess the nominal input frequency for the DPLL per source type.
    Adjust these numbers to match your actual design:

      - TIME_ONLY/TIME_AND_FREQ 1PPS sources → 1 Hz
      - FREQ_ONLY SMA inputs     → 10 MHz (assumed)
      - SyncE                    → 25 MHz (assumed)

    You can tweak these constants or even move them into the JSON later if needed.
    """
    if role in (TimeFreqRole.TIME_ONLY, TimeFreqRole.TIME_AND_FREQ):
        # 1PPS default
        return 1

    # FREQ_ONLY cases:
    if source_name == "SYNC_E":
        return 25_000_000  # adjust as appropriate for your switch
    if source_name.startswith("SMA"):
        return 10_000_000   # assumed external 10 MHz
    # Fallback
    return 10_000_000


# ---------- Configure board multiplexers via gpioset ----------
def configure_muxes(cfg: TimingConfig, dry_run: bool = False):
    """
    Configure all board-level multiplexers via gpioset based on the chosen config.

    Multiplexer layers (board-level, not inside the DPLL):

    1. SMA-side muxes:
        SMA1 -> GPS 1PPS direct OR to DPLL IN1 OR from DPLL Q11 (Ch6 1PPS)
        SMA2 -> CM4 1PPS direct OR to DPLL IN2 OR from DPLL OUT3
        SMA3 -> to DPLL IN3 OR from DPLL Q9 (Ch5 frequency-only)
        SMA4 -> fixed to DPLL IN4 (no mux)

    2. DPLL-input-side muxes:
        DPLL IN1 -> SyncE OR SMA1 path
        DPLL IN2 -> SMA2 path OR CM4 1PPS connection
        DPLL IN3 -> SMA3 path OR GPS 1PPS path
        DPLL IN4 -> SMA4 path (fixed)

    3. 1PPS destination (Q11, Channel 6):
        DPLL Q11 -> SMA1 path OR CM4 PPS

    4. Frequency output (Q9, Channel 5):
        DPLL Q9 -> SMA3 (frequency-only, phase-locked to SyncE)

    NOTE: V6 board change - Q9 and Q11 swapped:
      - Q9 (Ch5): now drives SMA3 for frequency output (SyncE aligned)
      - Q11 (Ch6): now drives CM4 1PPS / SMA1 for phase output
      This separates SyncE frequency from 1PPS phase alignment,
      allowing large phase jumps on Ch6 without disturbing SyncE 25MHz.

    This function decides all of that based on:
      - cfg.ptp_role (GM / CLIENT / NONE)
      - cfg.gps
      - cfg.cm4
      - cfg.synce
      - cfg.smas[*].direction / role

    NOTE:
      - GPIO numbers used here (chip, line) match your snippet for the SMA and CM4 muxes.
      - DPLL input mux GPIOs are placeholders: replace DPLL_INx_SEL_* with real lines.
    """

    ptp_role = PtpRole(cfg.ptp_role)

    # ----------------------------------------------------------------------
    # 1. CM4 <-> DPLL OUT2 / IN2 mux (your 10,11,14,16 gpios)
    # ----------------------------------------------------------------------
    sma1_cfg = next(s for s in cfg.smas if s.name == "SMA1")

    if ptp_role == PtpRole.GM:
        # CM4 is GM: DPLL Q11 (Ch6 1PPS) should go TO CM4 1PPS input.
        # CM4 multiplexer, output 1, CM4_MUX_SEL[1:0] = 2'b00
        print("CM4 is PTP GM: route DPLL Q11 (Ch6 1PPS) -> CM4 1PPS input")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "10=0", "11=0"], dry_run=dry_run)
        # V6: Enable DPLL Q11 (Ch6 1PPS) to go to CM4 (was Q9 in V5)
        # Q11 is now the 1PPS output, Q9 is frequency-only for SMA3
        print("Setup DPLL Q11 (Ch6 1pps) to go to CM4")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "16=1"], dry_run=dry_run)
        # enable CLK0N to come from SMA2
        print(f"Setup CLK0N to potentially come from SMA2")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "14=0"], dry_run=dry_run)
    else:
        # CM4 is CLIENT or NONE: CM4 1PPS goes TO DPLL input 2.
        # CM4 multiplexer, output 2, CM4_MUX_SEL[1:0] = 2'b01
        print("CM4 is PTP CLIENT/NONE: route CM4 1PPS -> DPLL IN2")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "10=1", "11=0"], dry_run=dry_run)        
        # 14=1: enable CM4 PPS path towards DPLL IN2 (per your comment)
        print("Setup CLK0N to potentially come from CM4")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "14=1"], dry_run=dry_run)
        # V6: enable Q11 (Ch6 1PPS) to go to SMA1 otherwise (was Q9 in V5)
        print(f"Setup Q11 (Ch6 1PPS) to potentially go to SMA1 ({get_user_sma_name('SMA1')})")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "16=0"], dry_run=dry_run)

    # ----------------------------------------------------------------------
    # 2. M.2 1PPS routing (always send to DPLL by default)
    # ----------------------------------------------------------------------
    print("Route M.2 1PPS to DPLL by default (output 1, M2_MUX_SEL=00)")
    run(["gpioset", "-t", "0", "-c", "gpiochip2", "6=0", "7=0"], dry_run=dry_run)
    
    print(f"Route DPLL Q10P to SMA2 ({get_user_sma_name('SMA2')}) by default")
    run(["gpioset", "-t", "0", "-c", "gpiochip2", "17=0"], dry_run=dry_run)

    # V6: Q9 (Ch5) now drives SMA3 for frequency output (SyncE aligned)
    # This is frequency-only output, phase-locked to SyncE channel
    print(f"V6: Route DPLL Q9 (Ch5 freq) to SMA3 ({get_user_sma_name('SMA3')}) by default")
    # (GPIO routing for Q9->SMA3 is typically handled by SMA3 mux below)

    print(f"Route DPLL CLK1N to SMA4 ({get_user_sma_name('SMA4')}) by default")
    run(["gpioset", "-t", "0", "-c", "gpiochip2", "15=0"], dry_run=dry_run)

    # ----------------------------------------------------------------------
    # 3. SMA front-end muxes (first layer, at the SMAs)
    #    SMA1_MUX_SEL[1:0] on gpiochip2 lines 0,1
    #      00 -> 'unused' / (we'll interpret as GPS direct or float)
    #      01 -> DPLL OUT2 -> SMA1
    #      10 -> SMA1 -> DPLL IN1
    #      11 -> (assume GPS direct -> SMA1)  <-- we'll use this mode if GPS present & SMA1 UNUSED
    #
    #    SMA2_MUX_SEL[1:0] on lines 2,3
    #      00 -> 'unused'/float (no CM4/SMA2/DPLL connection)
    #      01 -> DPLL OUT3 -> SMA2
    #      10 -> SMA2 -> DPLL IN2
    #      11 -> (reserved / CM4->SMA2 debug?) not used here
    #
    #    SMA3_MUX_SEL[1:0] on lines 4,5
    #      00 -> 'unused'/float
    #      10 -> SMA3 -> DPLL IN3
    #      11 -> DPLL Q9 (Ch5 freq) -> SMA3 (V6: frequency-only output, SyncE aligned)
    # ----------------------------------------------------------------------

    # --- SMA1 front-end mux ---
    if sma1_cfg.direction == "INPUT":
        # Route SMA1 -> DPLL IN1 , output 3 , SMA1_MUX_SEL[1:0] = 2'b10
        print(f"SMA1 ({get_user_sma_name('SMA1')}) as INPUT: route SMA1 -> DPLL IN1")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "0=0", "1=1"], dry_run=dry_run)
    elif sma1_cfg.direction == "OUTPUT":
        # Route DPLL OUT2 -> SMA1 , output 2, SMA1_MUX_SEL[1:0] = 2'b01
        print(f"SMA1 ({get_user_sma_name('SMA1')}) as OUTPUT: route DPLL OUT2 -> SMA1")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "0=1", "1=0"], dry_run=dry_run)
    else:
        # UNUSED:
        # If GPS is present and configured, we can use the "third" state as GPS->SMA1 (11),
        # otherwise leave it floating (00).
        print(f"SMA1 ({get_user_sma_name('SMA1')}) UNUSED: floating/unused (SMA1_MUX_SEL=00)")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "0=0", "1=0"], dry_run=dry_run)

    # --- SMA2 front-end mux ---
    sma2_cfg = next(s for s in cfg.smas if s.name == "SMA2")
    if sma2_cfg.direction == "INPUT":
        # SMA2 as input to DPLL IN2 , output 3, SMA2_MUX_SEL[1:0] = 2'b10
        print(f"SMA2 ({get_user_sma_name('SMA2')}) as INPUT: route SMA2 -> DPLL IN2")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "2=0", "3=1"], dry_run=dry_run)
    elif sma2_cfg.direction == "OUTPUT":
        # DPLL OUT3 -> SMA2 , output 2, SMA2_MUX_SEL[1:0] = 2'b01
        print(f"SMA2 ({get_user_sma_name('SMA2')}) as OUTPUT: route DPLL OUT3 -> SMA2")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "2=1", "3=0"], dry_run=dry_run)
    else:
        # UNUSED: route to unused / floating, output 1, SMA2_MUX_SEL[1:0] = 2'b00
        print(f"SMA2 ({get_user_sma_name('SMA2')}) UNUSED: floating/unused (SMA2_MUX_SEL=00)")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "2=0", "3=0"], dry_run=dry_run)

    # --- SMA3 front-end mux ---
    sma3_cfg = next(s for s in cfg.smas if s.name == "SMA3")
    if sma3_cfg.direction == "INPUT":
        # SMA3 as input to DPLL IN3, output 3, SMA3_MUX_SEL[1:0] = 2'b10
        print(f"SMA3 ({get_user_sma_name('SMA3')}) as INPUT: route SMA3 -> DPLL IN3")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "4=0", "5=1"], dry_run=dry_run)
    elif sma3_cfg.direction == "OUTPUT":
        # V6: DPLL Q9 (Ch5 freq) -> SMA3, SMA3_MUX_SEL[1:0] = 2'b11
        # Note: SMA3 output is FREQUENCY-ONLY (phase-locked to SyncE channel)
        # Phase adjustments happen on Ch6, not Ch5, so SyncE alignment is preserved
        print("SMA3 as OUTPUT: route DPLL Q9 (Ch5 freq-only) -> SMA3")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "4=1", "5=1"], dry_run=dry_run)
    else:
        # UNUSED: floating, output 1, SMA3_MUX_SEL[1:0] = 2'b00
        print("SMA3 UNUSED: floating/unused (SMA3_MUX_SEL=00)")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "4=0", "5=0"], dry_run=dry_run)

    # SMA4 has no mux; always input to DPLL IN4 → nothing to do front-end wise.

    # ----------------------------------------------------------------------
    # 4. DPLL INPUT muxes (second layer between DPLL and board)
    #
    #   DPLL IN1 -> SyncE output OR SMA1 path
    #   DPLL IN2 -> SMA2 path OR CM4 1PPS
    #   DPLL IN3 -> SMA3 path OR GPS 1PPS
    #   DPLL IN4 -> SMA4 path (fixed, no mux)
    #
    # Here we decide *which* source actually feeds each input, based on the
    # chosen references in cfg (SyncE, GPS, CM4, SMA inputs).
    #
    # NOTE: the GPIO names DPLL_INx_SEL_* are placeholders. Replace them
    # with your real device-tree lines.
    # ----------------------------------------------------------------------

    # --- DPLL IN1: SyncE vs SMA1 path ---
    use_synce = cfg.synce.used_as_source
    use_sma1_input = (sma1_cfg.direction == "INPUT" and sma1_cfg.role is not None)

    if use_synce and use_sma1_input:
        # This should already be disallowed by the logical layer (derive_plan),
        # but we add a loud print here too.
        print("WARNING: both SyncE and SMA1 are configured as references. "
              "Hardware only allows one on DPLL IN1. Prefer SyncE, ignoring SMA1 path.")
        use_sma1_input = False

    if use_synce:
        print("DPLL IN1 source: SyncE (board mux: SyncE -> DPLL IN1)")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "12=1"], dry_run=dry_run)
    elif use_sma1_input:
        print("DPLL IN1 source: SMA1 path (board mux: SMA1 -> DPLL IN1)")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "12=0"], dry_run=dry_run)
    else:
        # Default: if SyncE is physically always present, you may pick it.
        print("DPLL IN1 source: defaulting to SyncE (no SMA1 input selected)")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "12=1"], dry_run=dry_run)

    # --- DPLL IN2: SMA2 path vs CM4 1PPS ---
    use_cm4_src = cfg.cm4.used_as_source  # only true when CLIENT/NONE + user opted in
    use_sma2_input = (sma2_cfg.direction == "INPUT" and sma2_cfg.role is not None)

    if use_cm4_src and use_sma2_input:
        print("WARNING: CM4 PPS and SMA2 are both configured as DPLL IN2 candidates. "
              "Hardware only allows one. Preferring CM4 PPS, ignoring SMA2 path.")
        use_sma2_input = False

    if use_cm4_src:
        print("DPLL IN2 source: CM4 1PPS (board mux: CM4 PPS -> DPLL IN2)")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "14=1"], dry_run=dry_run)
    elif use_sma2_input:
        print("DPLL IN2 source: SMA2 path (board mux: SMA2 -> DPLL IN2)")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "14=0"], dry_run=dry_run)
    else:
        # If neither, you can default to CM4 path or SMA2 path as you like.
        print("DPLL IN2 source: defaulting to CM4 path (no SMA2 input selected and CM4 not used as ref)")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "14=1"], dry_run=dry_run)

    # --- DPLL IN3: SMA3 path vs GPS 1PPS ---
    use_gps = cfg.gps.present and cfg.gps.role is not None
    use_sma3_input = (sma3_cfg.direction == "INPUT" and sma3_cfg.role is not None)

    if use_gps and use_sma3_input:
        print("WARNING: GPS and SMA3 are both configured as references. "
              "Hardware only allows one on DPLL IN3. Preferring GPS, ignoring SMA3 path.")
        use_sma3_input = False

    if use_gps:
        print("DPLL IN3 source: GPS 1PPS (board mux: GPS -> DPLL IN3)")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "13=1"], dry_run=dry_run)
    elif use_sma3_input:
        print("DPLL IN3 source: SMA3 path (board mux: SMA3 -> DPLL IN3)")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "13=0"], dry_run=dry_run)
    else:
        # Default; if GPS hardware is present but not used as ref, you might still pick it
        print("DPLL IN3 source: defaulting to GPS path (no SMA3 input selected)")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "13=1"], dry_run=dry_run)

    # DPLL IN4 has no mux (always SMA4 path) -> nothing to do here.






# ---------- Configure DPLL inputs and channel priorities ----------
def configure_dpll_inputs(cfg: TimingConfig, dry_run: bool = False):
    """
    Use the derived plan (from config.py) to:
      - Configure input frequencies for each used DPLL *logical* input (1..4),
        but call dplltool with the *physical* indices (0, 8, 1, 9).
      - Configure channel 2 (frequency DPLL) input priorities/enables
      - Configure channels 5 and 6 (time DPLLs) input priorities/enables
      - Explicitly ENABLE only the inputs that are used, and DISABLE the rest.

    NOTE:
      - The plan and config talk about logical input indices 1..4.
      - This function maps those to physical indices:
            1 -> 0
            2 -> 8
            3 -> 1
            4 -> 9
        when calling dplltool.
      - Priorities from derive_plan() are 1-based (1 = highest); the DPLL
        wants 0-based, so we subtract 1 before calling dplltool.
    """

    plan = derive_plan(cfg)

    if plan.errors:
        print("Configuration errors from derive_plan():")
        for e in plan.errors:
            print("  ERROR:", e)
        raise SystemExit(1)

    # Map sources in the plan to DPLL logical input indices, checking for conflicts
    used_inputs: Dict[int, str] = {}  # logical_input_idx (1..4) -> source_name

    def register_source_input(source_name: str, role: TimeFreqRole, manual_freq_hz: int = None) -> int:
        # Logical index (1..4) based on your board wiring / source mapping
        logical_idx = map_source_to_input_idx(source_name)

        if logical_idx in used_inputs and used_inputs[logical_idx] != source_name:
            raise RuntimeError(
                f"Conflict: sources '{used_inputs[logical_idx]}' and '{source_name}' "
                f"both want DPLL logical input {logical_idx}. Adjust your config "
                f"(e.g. cannot use SYNC_E and SMA1 simultaneously as references)."
            )
        used_inputs[logical_idx] = source_name

        # Set nominal input frequency for this DPLL *physical* input
        if manual_freq_hz is not None:
            freq_hz = manual_freq_hz
        else:
            freq_hz = nominal_input_freq_hz(source_name, role)
            
        phys_idx = logical_to_physical_input_idx(logical_idx)

        print(f"  Configuring DPLL Input {logical_idx} (Phys {phys_idx}) for {source_name} @ {freq_hz} Hz")
        run(["dplltool", "set-input-freq", str(phys_idx), str(freq_hz)], dry_run=dry_run)
        return logical_idx

    # Collect unique sources from both lists to register them
    unique_sources: Dict[str, Tuple[int, TimeFreqRole]] = {} # name -> (idx, role)
    
    # We aggregate both lists to ensure we register every source used in the plan
    all_plan_inputs = plan.channel5_inputs + plan.channel6_inputs
    
    for src in all_plan_inputs:
        if src.name not in unique_sources:
            # Determine frequency
            freq_to_set = None
            
            # If it's an SMA, check if user provided a specific input frequency
            if src.name.startswith("SMA"):
                # find sma config in cfg.smas
                # Note: config.py ensures name match
                sma_cfg = next((s for s in cfg.smas if s.name == src.name), None)
                if sma_cfg and sma_cfg.frequency_hz is not None:
                     freq_to_set = sma_cfg.frequency_hz
            
            # Register this source's physical input frequency
            idx_logic = register_source_input(src.name, src.role, manual_freq_hz=freq_to_set)
            unique_sources[src.name] = (idx_logic, src.role)

    # ------------------------------------------------------------------
    # Channel 5 (Frequency DPLL) input priority list
    # ------------------------------------------------------------------
    for src in plan.channel5_inputs:
        logical_idx, _ = unique_sources[src.name]
        phys_idx = logical_to_physical_input_idx(logical_idx)

        # Plan uses 1-based priority in config, but we pass it as-is? 
        # Wait, the comment says: "Priorities from derive_plan() are 1-based... DPLL wants 0-based"
        # But `derive_plan` might be producing 0-based now?
        # Let's stick to the existing code's assumption that src.priority is what we want,
        # or maybe we need to be careful.
        # Looking at config.py wizard: "GPS priority (0 = highest) [0]" 
        # So priorities are 0-based integers.
        # The existing code had `if prio_hw < 0: prio_hw = 0`.
        
        prio_hw = src.priority
        if prio_hw < 0:
            prio_hw = 0

        run([
            "dplltool", "set-chan-input",
            "5",                   # Channel 5 (Freq)
            str(phys_idx),
            str(prio_hw),
            "enable",
        ], dry_run=dry_run)

    # ------------------------------------------------------------------
    # Channel 6 (Time/1PPS DPLL) input priority list
    # ------------------------------------------------------------------
    for src in plan.channel6_inputs:
        logical_idx, _ = unique_sources[src.name]
        phys_idx = logical_to_physical_input_idx(logical_idx)

        prio_hw = src.priority
        if prio_hw < 0:
            prio_hw = 0

        run([
            "dplltool", "set-chan-input",
            "6",                   # Channel 6 (Time)
            str(phys_idx),
            str(prio_hw),
            "enable",
        ], dry_run=dry_run)

    # ------------------------------------------------------------------
    # Combo Bus Slaving Logic
    # ------------------------------------------------------------------
    # Rule 1: If we have FREQ_ONLY sources (SyncE, SMA 10MHz), they are on Channel 5.
    #         Channel 5 is the Frequency Master. Channel 6 slaves to Channel 5.
    # Rule 2: If we have NO FREQ_ONLY sources, but we have TIME sources (GPS, PTP, SMA 1PPS),
    #         they are on Channel 6.
    #         Channel 6 is the Master (it has the references). Channel 5 slaves to Channel 6
    #         to provide SyncE/10MHz outputs derived from the Time source.
    # ------------------------------------------------------------------
    
    # Check for presence of FREQ_ONLY sources in Channel 5's plan
    # Compare enum-to-enum (InputSource.role is a TimeFreqRole enum, not a string)
    has_freq_only_source = any(s.role == TimeFreqRole.FREQ_ONLY for s in plan.channel5_inputs)
    
    # Check for any source on Channel 6 (Time/1PPS)
    has_time_source = len(plan.channel6_inputs) > 0

    if has_time_source:
        # Scenario A: Time Source Available (GPS, PTP, SMA 1PPS) -> Ch6 Independent
        # Even if SyncE is present on Ch5, we do NOT slave Ch6 to Ch5 because PTP 1PPS
        # often drifts relative to SyncE frequency (e.g. NIC oscillator wander vs SyncE).
        # Slaving creates a "two masters" conflict that causes phase drift or bouncing.
        # Ch6 should lock directly to its 1PPS input.
        print("\nCombo Bus Strategy: Time Source detected. Ch6 runs independently for stable phase lock.")
        
        # Disable Ch6 -> Ch5 (Ch6 is independent)
        configure_combo_slaving(slave=6, master=5, enable=False, dry_run=dry_run)
        
        # EXPERIMENT 2026-02-10:
        # Phase Slope Limit (PSL) Logic:
        # - If GPS is present (Grandmaster), use TIGHT filtering (10 ns/s).
        # - If PTP (CM4) is present (Client), use RELAXED filtering (100 ns/s) to track servo.
        # - If SMA is present (USER input), default to TIGHT (10 ns/s) unless user changes it manually.
        has_gps_source = any("GPS" in s.name for s in plan.channel6_inputs)
        has_ptp_source = any("CM4" in s.name for s in plan.channel6_inputs)
        
        if has_gps_source:
            psl_limit = 10
            reason = "GPS detected (Tight)"
        elif has_ptp_source:
            # Tuned for PTP Client (CM4 tracking):
            # - PSL: 300 ns/s (Track servo transients)
            # - Bandwidth: 100 mHz (Track wander without excessive noise integration)
            # - Damping: 4 (Slightly faster response)
            psl_limit = 300
            reason = "PTP/CM4 detected (Relaxed)"
            
            print("Experiment: Setting Ch6 Loop Bandwidth to 100 mHz (PTP tracking).")
            run(["dplltool", "set-loop-bw", "6", "100", "mHz"], dry_run=dry_run)
            
            print("Experiment: Setting Ch6 Damping Factor to 4.")
            run(["dplltool", "set-damp-factor", "6", "4"], dry_run=dry_run)
        else:
            psl_limit = 10
            reason = "Default/SMA (Tight)"
        
        print(f"Experiment: Setting Ch6 Phase Slope Limit to {psl_limit} ns/s ({reason}).")
        run(["dplltool", "set-psl", "6", str(psl_limit)], dry_run=dry_run)
        
        # If Freq source exists on Ch5 (Scenario A1: SyncE + PTP), Ch5 runs independently too.
        # If NO Freq source (Scenario A2: GPS/PTP only), Ch5 slaves to Ch6 to get synthesized frequency.
        if has_freq_only_source:
            print("  - Frequency Source detected (SyncE/10MHz). Ch5 runs independently.")
            configure_combo_slaving(slave=5, master=6, enable=False, dry_run=dry_run)
        else:
            print("  - No dedicated Freq Source. Ch5 slaves to Ch6 (Time) for synthesized frequency.")
            configure_combo_slaving(slave=5, master=6, enable=True, dry_run=dry_run)

    elif has_freq_only_source:
        # Scenario B: ONLY Frequency Source Available (SyncE/10MHz, no PPS)
        # Ch6 MUST slave to Ch5 to generate stable 1PPS/Time from the frequency reference.
        print("\nCombo Bus Strategy: Only Freq Source (SyncE/10MHz) detected. Ch6 slaves to Ch5.")
        configure_combo_slaving(slave=6, master=5, enable=True, dry_run=dry_run)
        configure_combo_slaving(slave=5, master=6, enable=False, dry_run=dry_run)

    else:
        # Scenario C: No sources at all?
        print("\nCombo Bus Strategy: No active sources. Disabling combo bus.")
        configure_combo_slaving(slave=6, master=5, enable=False, dry_run=dry_run)
        configure_combo_slaving(slave=5, master=6, enable=False, dry_run=dry_run)

    # ------------------------------------------------------------------
    # Explicitly enable / disable each DPLL input pin
    #
    # Inputs are logically numbered 1..4 here; we map them to physical
    # 0/8/1/9 when calling dplltool.
    # ------------------------------------------------------------------
    ALL_INPUTS = (1, 2, 3, 4)

    print("\nDPLL input enable state:")
    for logical_idx in ALL_INPUTS:
        phys_idx = logical_to_physical_input_idx(logical_idx)
        if logical_idx in used_inputs:
            print(
                f"  enabling DPLL input logical {logical_idx} "
                f"(physical {phys_idx}, source={used_inputs[logical_idx]})"
            )
            run(["dplltool", "set-input-enable", str(phys_idx), "enable"], dry_run=dry_run)
        else:
            print(
                f"  disabling unused DPLL input logical {logical_idx} "
                f"(physical {phys_idx})"
            )
            run(["dplltool", "set-input-enable", str(phys_idx), "disable"], dry_run=dry_run)
















# ---------- Helper: Configure SMA Output Divider ----------
def configure_sma_output_divider(output_idx: int, target_hz: int, dry_run: bool = False):
    """
    Calculate and apply the integer output divider for a given output index.
    Divider = DCO_FREQ_HZ / target_hz
    """
    if target_hz <= 0:
        print(f"Skipping output divider for {output_idx}: invalid target {target_hz} Hz")
        return

    divider = round(DCO_FREQ_HZ / target_hz)
    
    # Sanity checks
    if divider < 1: divider = 1
    if divider > 0xFFFFFFFF:
        print(f"WARNING: Calculated divider {divider} for {target_hz}Hz exceeds 32-bit limit!")
        divider = 0xFFFFFFFF

    actual_hz = DCO_FREQ_HZ / divider
    error_ppm = ((actual_hz - target_hz) / target_hz) * 1e6
    
    print(f"Configuring Output {output_idx} for {target_hz} Hz:")
    print(f"  DCO={DCO_FREQ_HZ} Hz, Divider={divider}")
    print(f"  Actual={actual_hz:.3f} Hz, Error={error_ppm:.3f} ppm")

    if abs(error_ppm) > 100:
        print("  WARNING: Large frequency error due to integer division!")

    run(["dplltool", "set-output-divider", str(output_idx), str(divider)], dry_run=dry_run)


# ---------- Helper: Configure Combo Bus Slaving ----------
def configure_combo_slaving(slave: int, master: int, enable: bool, dry_run: bool = False):
    """
    Configure a DPLL channel (slave) to track another channel (master) via Combo Bus.
    """
    state = "enable" if enable else "disable"
    print(f"Configuring Combo Bus: Channel {slave} slaves to Channel {master} -> {state}")
    run(["dplltool", "set-combo-slave", str(slave), str(master), state], dry_run=dry_run)


# ---------- Configure DPLL outputs (frequencies and routing) ----------
def configure_dpll_outputs(cfg: TimingConfig, dry_run: bool = False):
    """
    Configure DPLL Q10 and Q11 output frequencies based on SMA OUTPUT config.

    V6 Board Output Mapping:
      - DPLL Q9 (Ch5): Fixed 10MHz → SMA3 (frequency-only, SyncE aligned)
        * Configured in TCS/EEPROM, NOT via dplltool set-output-freq
      - DPLL Q10: → SMA2 (user-configurable frequency, no restrictions)
      - DPLL Q11 (Ch6): → CM4/SMA1 (phase-aligned output)
        * In GM mode: must be 1PPS (CM4 needs 1PPS input for disciplining)
        * In CLIENT/NONE mode: user-configurable frequency for SMA1

    dplltool set-output-freq <q10_hz> <q11_hz> sets Q10 and Q11 together.
    """

    ptp_role = PtpRole(cfg.ptp_role)

    # --- Q10 → SMA2 (user-configurable, no restrictions) ---
    sma2_cfg = next(s for s in cfg.smas if s.name == "SMA2")
    if sma2_cfg.direction == "OUTPUT" and sma2_cfg.frequency_hz is not None:
        q10_hz = float(sma2_cfg.frequency_hz)
    else:
        q10_hz = None  # not being driven / leave at default

    # --- Q11 (Ch6) → CM4/SMA1 ---
    # In GM mode: Q11 goes to CM4 and MUST be 1PPS for CM4 clock disciplining
    # In CLIENT/NONE mode: Q11 can go to SMA1 with user-configurable frequency
    sma1_cfg = next(s for s in cfg.smas if s.name == "SMA1")
    
    if ptp_role == PtpRole.GM:
        # CM4 is GM: Q11 feeds CM4 1PPS input, must be 1PPS
        q11_hz = 1.0
        print("PTP GM mode: Q11 forced to 1PPS for CM4 1PPS input")
    elif sma1_cfg.direction == "OUTPUT" and sma1_cfg.frequency_hz is not None:
        # CLIENT/NONE mode: Q11 goes to SMA1, use configured frequency
        q11_hz = float(sma1_cfg.frequency_hz)
    else:
        # Default to 1PPS if SMA1 not configured as output
        q11_hz = 1.0

    # --- Q9 (Ch5) → SMA3 (User SMA2) ---
    # Q9 uses output divider only (we don't touch its FOD).
    # Default is 10MHz from TCS/EEPROM, but user can override via output divider.
    sma3_cfg = next(s for s in cfg.smas if s.name == "SMA3")
    if sma3_cfg.direction == "OUTPUT":
        q9_freq = sma3_cfg.frequency_hz if sma3_cfg.frequency_hz is not None else 10_000_000
        if sma3_cfg.frequency_hz == 1:
            print("WARNING: SMA3 (Q9/Ch5) cannot provide phase-aligned 1PPS.")
            print("         Use SMA1 (Q11/Ch6) for phase-aligned 1PPS output instead.")
        print(f"Configuring Q9 (SMA3/User SMA2) output divider for {q9_freq} Hz:")
        configure_sma_output_divider(9, q9_freq, dry_run=dry_run)

    # If Q10 is not being used, default to 1Hz so we can still call set-output-freq
    if q10_hz is None:
        q10_hz = 1.0
        print("Q10 (SMA2) not configured as OUTPUT, defaulting to 1Hz")

    # Use the full FOD M/N method for Q10 and Q11 via set-output-freq.
    # This programs the Fractional Output Divider (FOD) inside the DPLL,
    # giving exact frequency synthesis (not limited to integer divider resolution).
    # Q9 (SMA3/Ch5) uses set-output-divider only, since we don't want to touch its FOD.
    print(f"\nConfiguring Q10/Q11 via FOD (set-output-freq):")
    print(f"  Q10 (SMA2/User SMA3): {q10_hz} Hz")
    print(f"  Q11 (SMA1/User SMA4): {q11_hz} Hz")
    run([
        "dplltool", "set-output-freq",
        str(q10_hz), str(q11_hz),
    ], dry_run=dry_run)


# ---------- High-level apply function ----------

def apply_timing(config_path: str, dry_run: bool = False):
    print(f"Loading timing config from: {config_path}")
    cfg = load_config(config_path)

    print("\n[1/3] Configuring board multiplexers (gpioset)...")
    configure_muxes(cfg, dry_run=dry_run)

    print("\n[2/3] Configuring DPLL inputs and channel priorities...")
    configure_dpll_inputs(cfg, dry_run=dry_run)

    print("\n[3/3] Configuring DPLL outputs...")
    configure_dpll_outputs(cfg, dry_run=dry_run)

    print("\nDone applying timing configuration.")


# ---------- CLI entrypoint ----------

def main():
    parser = argparse.ArgumentParser(
        description="Apply 8A34004 timing/DPLL configuration at boot."
    )
    parser.add_argument(
        "--config", "-c",
        default="/etc/dpll-config.json",
        help="Path to config JSON file (generated by config.py wizard).",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print commands without executing them.",
    )
    args = parser.parse_args()

    apply_timing(args.config, dry_run=args.dry_run)


if __name__ == "__main__":
    main()
