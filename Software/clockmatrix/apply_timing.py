#!/usr/bin/env python3
import argparse
import subprocess
from typing import Dict, Tuple

# Import your existing config logic
# Make sure config.py (the wizard/validate script) is in the same directory or PYTHONPATH.
from config import TimingConfig, load_config, derive_plan, PtpRole, TimeFreqRole


# ---------- Utility for running shell commands ----------

def run(cmd, dry_run: bool = False):
    print("RUN:", " ".join(cmd))
    if not dry_run:
        subprocess.run(cmd, check=True)


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
        SMA1 -> GPS 1PPS direct OR to DPLL IN1 OR from DPLL OUT2
        SMA2 -> CM4 1PPS direct OR to DPLL IN2 OR from DPLL OUT3
        SMA3 -> to DPLL IN3 OR from DPLL OUT4
        SMA4 -> fixed to DPLL IN4 (no mux)

    2. DPLL-input-side muxes:
        DPLL IN1 -> SyncE OR SMA1 path
        DPLL IN2 -> SMA2 path OR CM4 1PPS connection
        DPLL IN3 -> SMA3 path OR GPS 1PPS path
        DPLL IN4 -> SMA4 path (fixed)

    3. OUT2 destination:
        DPLL OUT2 -> SMA1 path OR CM4 PPS

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
        # CM4 is GM: DPLL OUT2 should go TO CM4 1PPS input.
        # CM4 multiplexer, output 1, CM4_MUX_SEL[1:0] = 2'b00
        print("CM4 is PTP GM: route DPLL OUT2 -> CM4 1PPS input")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "10=0", "11=0"], dry_run=dry_run)
        # Enable DPLL Q9P (1PPS) to go to CM4 (16=1, your note)
        print("Setup DPLL Q9P (1pps) to go to CM4")
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
        # enable Q9_P to go to SMA1 otherwise
        print("Setup Q9_P to potentially go to SMA1")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "16=0"], dry_run=dry_run)

    # ----------------------------------------------------------------------
    # 2. M.2 1PPS routing (always send to DPLL by default)
    # ----------------------------------------------------------------------
    print("Route M.2 1PPS to DPLL by default (output 1, M2_MUX_SEL=00)")
    run(["gpioset", "-t", "0", "-c", "gpiochip2", "6=0", "7=0"], dry_run=dry_run)
    
    print("Route DPLL Q10P to SMA2 by default")
    run(["gpioset", "-t", "0", "-c", "gpiochip2", "17=0"], dry_run=dry_run)

    print("Route DPLL CLK1N to SMA4 by default")
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
    #      11 -> DPLL OUT4 -> SMA3
    # ----------------------------------------------------------------------

    # --- SMA1 front-end mux ---
    if sma1_cfg.direction == "INPUT":
        # Route SMA1 -> DPLL IN1 , output 3 , SMA1_MUX_SEL[1:0] = 2'b10
        print("SMA1 as INPUT: route SMA1 -> DPLL IN1")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "0=0", "1=1"], dry_run=dry_run)
    elif sma1_cfg.direction == "OUTPUT":
        # Route DPLL OUT2 -> SMA1 , output 2, SMA1_MUX_SEL[1:0] = 2'b01
        print("SMA1 as OUTPUT: route DPLL OUT2 -> SMA1")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "0=1", "1=0"], dry_run=dry_run)
    else:
        # UNUSED:
        # If GPS is present and configured, we can use the "third" state as GPS->SMA1 (11),
        # otherwise leave it floating (00).
        print("SMA1 UNUSED: floating/unused (SMA1_MUX_SEL=00)")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "0=0", "1=0"], dry_run=dry_run)

    # --- SMA2 front-end mux ---
    sma2_cfg = next(s for s in cfg.smas if s.name == "SMA2")
    if sma2_cfg.direction == "INPUT":
        # SMA2 as input to DPLL IN2 , output 3, SMA2_MUX_SEL[1:0] = 2'b10
        print("SMA2 as INPUT: route SMA2 -> DPLL IN2")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "2=0", "3=1"], dry_run=dry_run)
    elif sma2_cfg.direction == "OUTPUT":
        # DPLL OUT3 -> SMA2 , output 2, SMA2_MUX_SEL[1:0] = 2'b01
        print("SMA2 as OUTPUT: route DPLL OUT3 -> SMA2")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "2=1", "3=0"], dry_run=dry_run)
    else:
        # UNUSED: route to unused / floating, output 1, SMA2_MUX_SEL[1:0] = 2'b00
        print("SMA2 UNUSED: floating/unused (SMA2_MUX_SEL=00)")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "2=0", "3=0"], dry_run=dry_run)

    # --- SMA3 front-end mux ---
    sma3_cfg = next(s for s in cfg.smas if s.name == "SMA3")
    if sma3_cfg.direction == "INPUT":
        # SMA3 as input to DPLL IN3, output 3, SMA3_MUX_SEL[1:0] = 2'b10
        print("SMA3 as INPUT: route SMA3 -> DPLL IN3")
        run(["gpioset", "-t", "0", "-c", "gpiochip2", "4=0", "5=1"], dry_run=dry_run)
    elif sma3_cfg.direction == "OUTPUT":
        # DPLL OUT4 -> SMA3, output 4, SMA3_MUX_SEL[1:0] = 2'b11
        print("SMA3 as OUTPUT: route DPLL OUT4 -> SMA3")
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

    def register_source_input(source_name: str, role: TimeFreqRole) -> int:
        # Logical index (1..4) based on your board wiring / source mapping
        logical_idx = map_source_to_input_idx(source_name)

        if logical_idx in used_inputs and used_inputs[logical_idx] != source_name:
            raise RuntimeError(
                f"Conflict: sources '{used_inputs[logical_idx]}' and '{source_name}' "
                f"both want DPLL logical input {logical_idx}. Adjust your config "
                f"(e.g. cannot use SYNC_E and SMA1 simultaneously as references)."
            )
        used_inputs[logical_idx] = source_name

        # Set nominal input frequency for this DPLL *physical* input, based on the source
        freq_hz = nominal_input_freq_hz(source_name, role)
        phys_idx = logical_to_physical_input_idx(logical_idx)

        run(["dplltool", "set-input-freq", str(phys_idx), str(freq_hz)], dry_run=dry_run)
        return logical_idx

    # Build mapping: source_name -> (logical_input_idx, role)
    src_to_input: Dict[str, Tuple[int, TimeFreqRole]] = {}

    # Collect from both channel2 and time channels (so we don't miss any)
    for src in plan.channel2_inputs + plan.time_channel_inputs:
        if src.name not in src_to_input:
            idx = register_source_input(src.name, src.role)
            src_to_input[src.name] = (idx, src.role)

    # ------------------------------------------------------------------
    # Channel 2 (frequency DPLL) input priority list
    # ------------------------------------------------------------------
    for src in plan.channel2_inputs:
        logical_idx, _ = src_to_input[src.name]  # logical 1..4
        phys_idx = logical_to_physical_input_idx(logical_idx)

        # Plan uses 0-based priority (0 = highest); DPLL expects 0-based
        prio_hw = src.priority
        if prio_hw < 0:
            prio_hw = 0

        run([
            "dplltool", "set-chan-input",
            "2",                   # Channel 2
            str(phys_idx),
            str(prio_hw),
            "enable",
        ], dry_run=dry_run)

    # ------------------------------------------------------------------
    # Time channels (5 & 6) input priority list
    # ------------------------------------------------------------------
    for src in plan.time_channel_inputs:
        logical_idx, _ = src_to_input[src.name]
        phys_idx = logical_to_physical_input_idx(logical_idx)

        prio_hw = src.priority 
        if prio_hw < 0:
            prio_hw = 0

        for chan in (5, 6):
            run([
                "dplltool", "set-chan-input",
                str(chan),
                str(phys_idx),
                str(prio_hw),
                "enable",
            ], dry_run=dry_run)

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















# ---------- Configure DPLL outputs (frequencies and routing) ----------
def configure_dpll_outputs(cfg: TimingConfig, dry_run: bool = False):
    """
    Configure DPLL outputs 3 and 4 based on SMA OUTPUT config.

    Board mapping recap:
      - DPLL OUT2: fixed 1 Hz (1PPS) for CM4/SMA1 (configured elsewhere / statically)
      - DPLL OUT3: drives SMA2
      - DPLL OUT4: drives SMA3
      - SMA4: no output path

    With the new dplltool CLI, set-output-freq always sets OUT3 and OUT4 together:
      dplltool set-output-freq <freq3_hz> <freq4_hz>
    """

    ptp_role = PtpRole(cfg.ptp_role)

    # --- OUT3 → SMA2 ---
    sma2_cfg = next(s for s in cfg.smas if s.name == "SMA2")
    if sma2_cfg.direction == "OUTPUT" and sma2_cfg.frequency_hz is not None:
        out3_hz = float(sma2_cfg.frequency_hz)
    else:
        out3_hz = None  # not being driven / leave at default

    # --- OUT4 → SMA3 ---
    sma3_cfg = next(s for s in cfg.smas if s.name == "SMA3")
    if sma3_cfg.direction == "OUTPUT" and sma3_cfg.frequency_hz is not None:
        out4_hz = float(sma3_cfg.frequency_hz)
    else:
        out4_hz = None

    # If neither OUT3 nor OUT4 is used, don't touch them
    if out3_hz is None and out4_hz is None:
        return

    # Decide what to do when only one of them is used.
    # Easiest: fill the unused one with a safe default (e.g. its nominal/static value).
    # For now, assume 1 Hz as a harmless placeholder; you can change this to whatever
    # matches your TCS/EEPROM default for the unused output.
    if out3_hz is None:
        out3_hz = 1.0
    if out4_hz is None:
        out4_hz = 1.0

    run(
        [
            "dplltool",
            "set-output-freq",
            str(out3_hz),
            str(out4_hz),
        ],
        dry_run=dry_run,
    )


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
