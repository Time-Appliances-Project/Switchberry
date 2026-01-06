#!/usr/bin/env python3
import json
import argparse
from dataclasses import dataclass, asdict
from enum import Enum
from typing import List, Optional, Dict


# ---------- Enums ----------

class PtpRole(Enum):
    GM = "GM"
    CLIENT = "CLIENT"
    NONE = "NONE"


class TimeFreqRole(Enum):
    TIME_ONLY = "TIME_ONLY"
    FREQ_ONLY = "FREQ_ONLY"
    TIME_AND_FREQ = "TIME_AND_FREQ"


class SmaDirection(Enum):
    INPUT = "INPUT"
    OUTPUT = "OUTPUT"
    UNUSED = "UNUSED"


# ---------- Dataclasses for config ----------

@dataclass
class GpsConfig:
    present: bool
    role: Optional[str] = None        # TimeFreqRole value or None if ignored
    priority: Optional[int] = None    # 1 = highest (applied to whichever domain(s) it affects)


@dataclass
class Cm4Config:
    used_as_source: bool
    role: Optional[str] = None        # usually TIME_ONLY or TIME_AND_FREQ
    priority: Optional[int] = None    # when used as source


@dataclass
class SyncEConfig:
    used_as_source: bool
    priority: Optional[int] = None    # FREQ_ONLY source
    recover_port: Optional[int] = None    # FREQ_ONLY source


@dataclass
class SmaConfig:
    name: str                         # "SMA1".."SMA4"
    direction: str                    # SmaDirection value
    role: Optional[str] = None        # TimeFreqRole (if INPUT)
    priority: Optional[int] = None    # priority (if INPUT)
    frequency_hz: Optional[int] = None  # desired output frequency (if OUTPUT)


@dataclass
class TimingConfig:
    ptp_role: str                     # PtpRole value
    gps: GpsConfig
    cm4: Cm4Config
    synce: SyncEConfig
    smas: List[SmaConfig]


# ---------- Helper: serialize / deserialize ----------

def config_to_json_dict(cfg: TimingConfig) -> dict:
    return {
        "ptp_role": cfg.ptp_role,
        "gps": asdict(cfg.gps),
        "cm4": asdict(cfg.cm4),
        "synce": asdict(cfg.synce),
        "smas": [asdict(s) for s in cfg.smas],
    }


def config_from_json_dict(data: dict) -> TimingConfig:
    gps = GpsConfig(**data["gps"])
    cm4 = Cm4Config(**data["cm4"])
    synce = SyncEConfig(**data["synce"])
    smas = [SmaConfig(**s) for s in data["smas"]]
    return TimingConfig(
        ptp_role=data["ptp_role"],
        gps=gps,
        cm4=cm4,
        synce=synce,
        smas=smas,
    )


def save_config(path: str, cfg: TimingConfig) -> None:
    with open(path, "w") as f:
        json.dump(config_to_json_dict(cfg), f, indent=2)
    print(f"\nConfig written to {path}")


def load_config(path: str) -> TimingConfig:
    with open(path, "r") as f:
        data = json.load(f)
    return config_from_json_dict(data)


# ---------- Internal representation of an input source ----------

@dataclass
class InputSource:
    name: str            # "GPS", "CM4", "SYNC_E", "SMA1"..
    role: TimeFreqRole
    priority: int        # lower = higher priority


# ---------- Derivation + validation logic ----------

@dataclass
class DerivedPlan:
    channel2_inputs: List[InputSource]     # frequency DPLL (ch2)
    time_channel_inputs: List[InputSource] # common list for channels 5 & 6
    warnings: List[str]
    errors: List[str]


def derive_plan(cfg: TimingConfig) -> DerivedPlan:
    """
    Rules:

      - Channel 2 is the FREQUENCY DPLL.
      - Channels 5 & 6 are TIME/TOD (1PPS) consumers.

      - If any input (incl. GPS) is TIME_AND_FREQ:
          * Disable channel 2 input list (ch2 follows 5 & 6 via combo bus).
          * TIME_AND_FREQ inputs go into the time-channel priority list.

      - If any input is FREQ_ONLY:
          * They feed channel 2 input priority list (only if no TIME_AND_FREQ).

      - If any input is TIME_ONLY:
          * They go into the time-channel priority list.
          * If there is no frequency source at all, warn.
    """

    warnings: List[str] = []
    errors: List[str] = []

    ptp_role = PtpRole(cfg.ptp_role)

    # Build a flat list of InputSource from GPS, CM4, SyncE, and SMA inputs
    inputs: List[InputSource] = []

    # GPS
    if cfg.gps.present and cfg.gps.role is not None:
        inputs.append(
            InputSource(
                name="GPS",
                role=TimeFreqRole(cfg.gps.role),
                priority=cfg.gps.priority if cfg.gps.priority is not None else 999,
            )
        )

    # CM4 (when used as source, usually CLIENT or NONE)
    if cfg.cm4.used_as_source and cfg.cm4.role is not None:
        inputs.append(
            InputSource(
                name="CM4",
                role=TimeFreqRole(cfg.cm4.role),
                priority=cfg.cm4.priority if cfg.cm4.priority is not None else 999,
            )
        )

    # SyncE (frequency only)
    if cfg.synce.used_as_source:
        prio = cfg.synce.priority if cfg.synce.priority is not None else 999
        inputs.append(
            InputSource(
                name="SYNC_E",
                role=TimeFreqRole.FREQ_ONLY,
                priority=prio,
            )
        )

    # SMA inputs
    for sma in cfg.smas:
        if sma.direction == SmaDirection.INPUT.value:
            if sma.role is None or sma.priority is None:
                errors.append(f"{sma.name} is INPUT but role/priority is missing.")
                continue
            inputs.append(
                InputSource(
                    name=sma.name,
                    role=TimeFreqRole(sma.role),
                    priority=sma.priority,
                )
            )

    # Partition by role
    time_freq_sources = [s for s in inputs if s.role == TimeFreqRole.TIME_AND_FREQ]
    freq_only_sources = [s for s in inputs if s.role == TimeFreqRole.FREQ_ONLY]
    time_only_sources = [s for s in inputs if s.role == TimeFreqRole.TIME_ONLY]

    # Sort each by priority (ascending = higher priority)
    time_freq_sources.sort(key=lambda s: s.priority)
    freq_only_sources.sort(key=lambda s: s.priority)
    time_only_sources.sort(key=lambda s: s.priority)

    channel2_inputs: List[InputSource] = []
    time_channel_inputs: List[InputSource] = []

    # 1) TIME_AND_FREQ exists -> channel 2 follows time channels
    if time_freq_sources:
        channel2_inputs = []  # disabled explicitly
        time_channel_inputs.extend(time_freq_sources)
        if freq_only_sources:
            warnings.append(
                "TIME_AND_FREQ sources present: DPLL channel 2 input list is disabled; "
                "frequency-only sources will not feed channel 2 in this plan. "
                "If you want channel 2 to use external FREQ_ONLY refs, set your "
                "TIME_AND_FREQ sources (e.g. GPS) to TIME_ONLY."
            )
    else:
        # 2) No TIME_AND_FREQ -> FREQ_ONLY feeds channel 2
        channel2_inputs.extend(freq_only_sources)

    # 3) TIME_ONLY sources -> time channels (5 & 6)
    time_channel_inputs.extend(time_only_sources)
    time_channel_inputs.sort(key=lambda s: s.priority)

    # Sanity checks
    # a) GM must have at least one time source
    if ptp_role == PtpRole.GM and not time_channel_inputs:
        errors.append(
            "CM4 is configured as PTP GM, but there are no TIME or TIME_AND_FREQ sources "
            "assigned to the time channels (5 & 6)."
        )

    # b) TIME_ONLY but no frequency source at all
    has_any_freq_source = bool(time_freq_sources or freq_only_sources)
    if time_only_sources and not has_any_freq_source:
        warnings.append(
            "There are TIME_ONLY sources but no TIME_AND_FREQ or FREQ_ONLY sources. "
            "DPLL channel 2 will have no explicit frequency reference and may free-run."
        )

    # c) Check duplicate priorities per role set
    def find_duplicate_priorities(sources: List[InputSource], label: str):
        seen: Dict[int, List[str]] = {}
        for s in sources:
            seen.setdefault(s.priority, []).append(s.name)
        for prio, names in seen.items():
            if len(names) > 1:
                warnings.append(
                    f"{label}: multiple sources share priority {prio}: {', '.join(names)}."
                )

    find_duplicate_priorities(time_freq_sources, "TIME_AND_FREQ inputs")
    find_duplicate_priorities(freq_only_sources, "FREQ_ONLY inputs")
    find_duplicate_priorities(time_only_sources, "TIME_ONLY inputs")

    # d) SMA consistency checks + special SMA1 rules
    for sma in cfg.smas:
        if sma.direction == SmaDirection.INPUT.value:
            if sma.frequency_hz is not None:
                warnings.append(
                    f"{sma.name}: frequency_hz is set but direction is INPUT; "
                    "frequency_hz is only used for OUTPUT."
                )
        elif sma.direction == SmaDirection.OUTPUT.value:
            # SMA1 special: shares path with CM4 PPS, only 1PPS, and not allowed when GM
            if sma.name == "SMA1":
                if ptp_role == PtpRole.GM:
                    errors.append(
                        "SMA1 is configured as OUTPUT, but CM4 is PTP GM. "
                        "The SMA1 output shares the CM4 1PPS path, so SMA1 "
                        "cannot be used as an output in GM mode."
                    )
                if sma.frequency_hz not in (None, 1):
                    warnings.append(
                        "SMA1 is OUTPUT and frequency_hz is not 1; "
                        "hardware only supports 1PPS on SMA1. Treating SMA1 as 1PPS."
                    )
            else:
                if sma.frequency_hz is None:
                    warnings.append(
                        f"{sma.name}: OUTPUT selected but frequency_hz is not set."
                    )

            if sma.role is not None or sma.priority is not None:
                warnings.append(
                    f"{sma.name}: OUTPUT selected, but input role/priority are set. "
                    "They will be ignored."
                )

    return DerivedPlan(
        channel2_inputs=channel2_inputs,
        time_channel_inputs=time_channel_inputs,
        warnings=warnings,
        errors=errors,
    )


def validate_config(cfg: TimingConfig) -> DerivedPlan:
    plan = derive_plan(cfg)

    # If any TIME_AND_FREQ source exists, channel 2 is not used in this plan.
    has_time_and_freq = any(
        s.role == TimeFreqRole.TIME_AND_FREQ for s in plan.time_channel_inputs
    )

    # Track whether the user configured any FREQ_ONLY sources in the *config*
    # (these may be ignored if TIME_AND_FREQ is present).
    configured_freq_only = []

    if cfg.synce.used_as_source:
        configured_freq_only.append("SYNC_E")

    for sma in cfg.smas:
        if sma.direction == SmaDirection.INPUT.value and sma.role == TimeFreqRole.FREQ_ONLY.value:
            configured_freq_only.append(sma.name)

    print("=== Derived 8A34004 DPLL Plan ===")
    print(f"PTP role: {cfg.ptp_role}")

    # ---- SyncE summary ----
    if cfg.synce.used_as_source:
        synce_prio = cfg.synce.priority if cfg.synce.priority is not None else "unset"

        # recover_port is new; allow older configs that don't have it yet.
        synce_port = getattr(cfg.synce, "recover_port", None)
        if synce_port is None:
            print(f"SyncE: enabled (front-panel recover_port=UNSET, priority={synce_prio})")
        else:
            print(f"SyncE: enabled (front-panel recover_port={synce_port}, priority={synce_prio})")
    else:
        print("SyncE: disabled")

    # ---- High-level mode note ----
    print("\nDPLL mode summary:")
    if has_time_and_freq:
        print("  TIME_AND_FREQ source present -> combined time+frequency mode.")
        print("  Channels 5 & 6 lock to the TIME_AND_FREQ source(s) for phase/time,")
        print("  and frequency tracking is derived internally from that same reference.")
        print("  Channel 2 is NOT used as an external frequency lock point in this mode.")
        if configured_freq_only:
            print("  NOTE: FREQ_ONLY sources are configured but ignored while TIME_AND_FREQ is present.")
            print("        To use channel 2 with external frequency (e.g. SyncE), set time sources to TIME_ONLY.")
    else:
        print("  No TIME_AND_FREQ sources -> split time/frequency mode.")
        print("  Channels 5 & 6 lock to the time source(s) (TIME_ONLY).")
        if plan.channel2_inputs:
            print("  Channel 2 locks to frequency-only source(s) (FREQ_ONLY), and can provide frequency")
            print("  tracking to channels 5 & 6 via the internal combo bus.")
        else:
            print("  No FREQ_ONLY source configured -> no external frequency lock for channel 2 in this plan.")
            print("  (System will effectively freerun on the local oscillator unless you add SyncE/FREQ_ONLY input.)")

    # ---- Channel 2 list ----
    print("\nChannel 2 (frequency DPLL) input priority list (frequency domain priority):")
    if has_time_and_freq:
        print("  (disabled: TIME_AND_FREQ sources present; channel 2 not used as a separate lock point)")
    else:
        if plan.channel2_inputs:
            for s in plan.channel2_inputs:
                print(f"  priority {s.priority}: {s.name} ({s.role.value})")
        else:
            print("  (no frequency-only sources configured)")

    # ---- Channels 5 & 6 list ----
    print("\nChannels 5 & 6 (time/TOD DPLLs) input priority list (time domain priority):")
    if plan.time_channel_inputs:
        for s in plan.time_channel_inputs:
            print(f"  priority {s.priority}: {s.name} ({s.role.value})")
    else:
        print("  (no time sources configured)")

    # ---- SMA summary (existing behavior) ----
    print("\nSMA configuration:")
    for sma in cfg.smas:
        if sma.direction == SmaDirection.INPUT.value:
            print(f"  {sma.name}: INPUT, role={sma.role}, priority={sma.priority}")
        elif sma.direction == SmaDirection.OUTPUT.value:
            if sma.name == "SMA1":
                print(f"  {sma.name}: OUTPUT, 1PPS (shares path with CM4 1PPS)")
            else:
                print(f"  {sma.name}: OUTPUT, frequency={sma.frequency_hz} Hz")
        else:
            print(f"  {sma.name}: UNUSED")

    if plan.warnings:
        print("\nWarnings:")
        for w in plan.warnings:
            print(f"  - {w}")

    if plan.errors:
        print("\nErrors:")
        for e in plan.errors:
            print(f"  - {e}")
    else:
        print("\nConfig is valid (no errors).")

    return plan




# ---------- Interactive wizard ----------

def run_wizard(path: str) -> None:
    print("8A34004 Timing / DPLL configuration wizard")
    print("------------------------------------------")
    print("This wizard builds a JSON config that says:")
    print("  - CM4 PTP role (GM / CLIENT / NONE)")
    print("  - Which signals provide TIME and/or FREQUENCY references:")
    print("      * GPS 1PPS, CM4 1PPS, SyncE, SMA inputs")
    print("  - SMA1..SMA4 as inputs or outputs.")
    print()
    print("Important:")
    print("  • The DPLL has separate TIME and FREQUENCY priority lists:")
    print("      - TIME domain: channels 5 & 6 (1PPS / TOD consumers)")
    print("      - FREQUENCY domain: channel 2 (frequency DPLL)")
    print("  • Each reference gets a single 'priority' number;")
    print("    that priority is applied to whichever domain(s) it participates in,")
    print("    based on its ROLE:")
    print("      - TIME_ONLY       → affects time priority only")
    print("      - FREQ_ONLY       → affects frequency priority only")
    print("      - TIME_AND_FREQ   → affects both")
    print()
    print("SMA rules:")
    print("  - SMA1 output shares the same path as the CM4 1PPS:")
    print("      * If CM4 is PTP GM, SMA1 CANNOT be used as an output.")
    print("      * SMA1 output can only be 1PPS, not arbitrary frequency.")
    print("  - SMA4 is input-only and cannot be used as an output.\n")

    # Step 1: CM4 PTP role
    print("Step 1: CM4 PTP role")
    print("  GM     : CM4 is a PTP Grandmaster; PPS to CM4 comes from DPLL.")
    print("  CLIENT : CM4 is PTP client; PPS from CM4 can go to DPLL as a time source.")
    print("  NONE   : PTP not used / don't care; treat CM4 PPS as optional input only.")
    role_str = input("PTP role (GM/CLIENT/NONE) [NONE]: ").strip().upper() or "NONE"
    if role_str not in ("GM", "CLIENT", "NONE"):
        print("Unknown role, defaulting to NONE.")
        role_str = "NONE"

    # Step 2: GPS
    print("\nStep 2: GPS presence and usage")
    ans = input("Is GPS hardware present? (y/N): ").strip().lower()
    gps_present = ans == "y"
    gps_role = None
    gps_priority = None

    if gps_present:
        print("GPS usage options:")
        print("  A: GPS 1PPS is time AND frequency source (TIME_AND_FREQ)")
        print("  B: GPS 1PPS is TIME ONLY (frequency comes from somewhere else)")
        print("  C: GPS 1PPS is ignored")
        mode = input("Choose GPS mode (A/B/C) [A]: ").strip().upper() or "A"
        if mode == "C":
            gps_role = None
            gps_priority = None
        else:
            if mode == "A":
                gps_role = TimeFreqRole.TIME_AND_FREQ.value
            else:
                gps_role = TimeFreqRole.TIME_ONLY.value
            print("GPS priority applies to:")
            print("  - Time domain (channels 5 & 6) always, if GPS is TIME_*")
            print("  - Frequency domain (channel 2) as well, if GPS is TIME_AND_FREQ.")
            pr = input("GPS priority (1 = highest) [1]: ").strip() or "1"
            try:
                gps_priority = int(pr)
            except ValueError:
                gps_priority = 1
    gps_cfg = GpsConfig(present=gps_present, role=gps_role, priority=gps_priority)

    # Step 3: CM4 as time source (client/none)
    print("\n")
    if role_str in ("CLIENT"):
        print("PTP Client operation, help define priority, should PTP be top priority? Or something else?")
        print("CM4 PPS will typically be TIME_ONLY (phase/time), not a freq reference.")
        role_default = TimeFreqRole.TIME_ONLY.value
        r = input(f"Role for CM4 PPS (TIME_ONLY/TIME_AND_FREQ) [{role_default}]: ").strip().upper() or role_default
        if r not in ("TIME_ONLY", "TIME_AND_FREQ"):
            r = role_default
        print("CM4 priority applies to:")
        print("  - Time domain only (unless you pick TIME_AND_FREQ).")
        pr = input("CM4 PPS priority (1 = highest) [2]: ").strip() or "2"
        try:
            cm4_prio = int(pr)
        except ValueError:
            cm4_prio = 2
        cm4_cfg = Cm4Config(
            used_as_source=True,
            role=r,
            priority=cm4_prio,
        )
    else:
        # GM: CM4 is sink of PPS, not source
        print("CM4 is configured as PTP GM or not doing PTP at all.")
        print("In this mode, CM4 PPS is disciplined by the DPLL,")
        print("and PPS from the DPLL (via the SMA1/CM4 path) is used as the input to CM4.")
        print("Therefore, CM4 is not configured here as a time reference *into* the DPLL.")
        cm4_cfg = Cm4Config(used_as_source=False, role=None, priority=None)

    # Step 4: SyncE
    print("\nStep 4: SyncE as frequency-only source")
    ans = input("Use SyncE as a frequency-only source (FREQ_ONLY)? (y/N): ").strip().lower()
    if ans == "y":
        pr = input("SyncE frequency priority (1 = highest) [2]: ").strip() or "2"
        try:
            synce_prio = int(pr)
        except ValueError:
            synce_prio = 2

        p = input("SyncE front-panel recovery port (1-5) [1]: ").strip() or "1"
        try:
            synce_port = int(p)
        except ValueError:
            synce_port = 1
        if synce_port < 1 or synce_port > 5:
            print("  Invalid port selection, using port 1.")
            synce_port = 1

        synce_cfg = SyncEConfig(used_as_source=True, priority=synce_prio, recover_port=synce_port)
    else:
        synce_cfg = SyncEConfig(used_as_source=False, priority=None, recover_port=None)

    # Step 5: SMAs
    print("\nStep 5: SMA configuration (SMA1..SMA4)")
    print("Each SMA can be:")
    print("  INPUT  - used as a time/freq reference into the DPLL.")
    print("           - TIME_ONLY or TIME_AND_FREQ generally means 1PPS.")
    print("           - FREQ_ONLY generally means a continuous clock (e.g. 10 MHz).")
    print("           You must provide a PRIORITY when used as input.")
    print("  OUTPUT - driven by the DPLL with a frequency you choose (Hz, 1..250e6).")
    print("           NOTE: SMA1 OUTPUT is special and only supports 1PPS,")
    print("                 and SMA1 OUTPUT is not allowed when CM4 is PTP GM.")
    print("  UNUSED - not connected to DPLL in this config.")
    print("  NOTE: SMA4 is input-only and cannot be used as OUTPUT.\n")

    smas: List[SmaConfig] = []
    for i in range(1, 5):
        name = f"SMA{i}"
        print(f"\nConfiguring {name}:")
        if name == "SMA1" and role_str == "GM":
            print("  Note: CM4 is PTP GM, so SMA1 cannot be used as an OUTPUT.")
            print("        (it shares the CM4 1PPS path). You may use it as INPUT or UNUSED.")
            prompt = "  Direction (INPUT/UNUSED) [UNUSED]: "
            allowed = {"INPUT", "UNUSED"}
        elif name == "SMA4":
            print("  Note: SMA4 is input-only. You may use it as INPUT or UNUSED.")
            prompt = "  Direction (INPUT/UNUSED) [UNUSED]: "
            allowed = {"INPUT", "UNUSED"}
        else:
            prompt = "  Direction (INPUT/OUTPUT/UNUSED) [UNUSED]: "
            allowed = {"INPUT", "OUTPUT", "UNUSED"}

        d = input(prompt).strip().upper() or "UNUSED"
        if d not in allowed:
            print("  Unknown or invalid direction for this mode, defaulting to UNUSED.")
            d = "UNUSED"

        if d == "INPUT":
            r = input("  Role (TIME_ONLY/FREQ_ONLY/TIME_AND_FREQ) [TIME_AND_FREQ]: ").strip().upper() or "TIME_AND_FREQ"
            if r not in ("TIME_ONLY", "FREQ_ONLY", "TIME_AND_FREQ"):
                print("  Unknown role, defaulting to TIME_AND_FREQ.")
                r = "TIME_AND_FREQ"
            pr = input("  Priority (1 = highest) [3]: ").strip() or "3"
            try:
                prio = int(pr)
            except ValueError:
                prio = 3
            smas.append(
                SmaConfig(
                    name=name,
                    direction=SmaDirection.INPUT.value,
                    role=r,
                    priority=prio,
                    frequency_hz=None,
                )
            )
        elif d == "OUTPUT":
            if name == "SMA1":
                # OUTPUT only allowed here if CM4 is not GM, still 1PPS-only
                print("  SMA1 OUTPUT is limited to 1PPS. Setting frequency to 1 Hz.")
                smas.append(
                    SmaConfig(
                        name=name,
                        direction=SmaDirection.OUTPUT.value,
                        role=None,
                        priority=None,
                        frequency_hz=1,
                    )
                )
            else:
                while True:
                    f_str = input("  Desired output frequency in Hz (1 to 250000000): ").strip()
                    try:
                        freq = int(f_str)
                    except ValueError:
                        print("    Please enter an integer.")
                        continue
                    if not (1 <= freq <= 250_000_000):
                        print("    Frequency must be between 1 and 250000000 Hz.")
                        continue
                    break
                smas.append(
                    SmaConfig(
                        name=name,
                        direction=SmaDirection.OUTPUT.value,
                        role=None,
                        priority=None,
                        frequency_hz=freq,
                    )
                )
        else:
            smas.append(
                SmaConfig(
                    name=name,
                    direction=SmaDirection.UNUSED.value,
                    role=None,
                    priority=None,
                    frequency_hz=None,
                )
            )

    cfg = TimingConfig(
        ptp_role=role_str,
        gps=gps_cfg,
        cm4=cm4_cfg,
        synce=synce_cfg,
        smas=smas,
    )
    save_config(path, cfg)
    print("\nYou can now run:")
    print(f"  python3 config.py validate -c {path}")
    print("to see how this maps into DPLL channel priorities (ch2 vs ch5 & ch6).")


# ---------- CLI ----------

def main():
    parser = argparse.ArgumentParser(description="8A34004 timing / DPLL config generator + validator")
    sub = parser.add_subparsers(dest="cmd", required=True)

    # Each subcommand gets its own --config/-c
    p_wizard = sub.add_parser("wizard", help="Run interactive wizard to create config file")
    p_wizard.add_argument("--config", "-c", default="dpll-config.json",
                          help="Path to write config JSON")

    p_validate = sub.add_parser("validate", help="Validate config and show derived channel input lists")
    p_validate.add_argument("--config", "-c", default="dpll-config.json",
                            help="Path to config JSON file")

    p_show = sub.add_parser("show-derived", help="Show derived plan only (no extra wording)")
    p_show.add_argument("--config", "-c", default="dpll-config.json",
                        help="Path to config JSON file")

    args = parser.parse_args()

    if args.cmd == "wizard":
        run_wizard(args.config)
    elif args.cmd == "validate":
        cfg = load_config(args.config)
        plan = validate_config(cfg)
        if plan.errors:
            exit(1)
    elif args.cmd == "show-derived":
        cfg = load_config(args.config)
        plan = derive_plan(cfg)
        print("Channel 2 inputs:", [(s.name, s.role.value, s.priority) for s in plan.channel2_inputs])
        print("Time channel inputs (ch5 & ch6):", [(s.name, s.role.value, s.priority) for s in plan.time_channel_inputs])
        if plan.warnings:
            print("Warnings:", plan.warnings)
        if plan.errors:
            print("Errors:", plan.errors)


if __name__ == "__main__":
    main()
