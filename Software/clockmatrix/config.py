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
    NONE = "NONE"


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
    frequency_hz: Optional[int] = None  # desired output frequency (if OUTPUT), also used for input


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
    """
    V6 DPLL Channel Architecture:
      - Channel 5: FREQUENCY DPLL - locks to SyncE or frequency inputs (FREQ_ONLY sources)
                   Also handles TIME_AND_FREQ sources (combined time+freq reference)
      - Channel 6: TIME/1PPS DPLL - locks to 1PPS sources (TIME_ONLY)
                   Combo bus slave of Channel 5 for frequency tracking
      - Channel 2: Not used in the V6 timing path (legacy, not configured by this tool)

    Q9 (on Ch5) drives SMA3 at fixed 10MHz.
    Q11 (on Ch6) drives CM4 1PPS (GM mode) or SMA1 (CLIENT mode).
    """
    channel5_inputs: List[InputSource]     # frequency DPLL (Ch5) - SyncE, FREQ_ONLY, or TIME_AND_FREQ
    channel6_inputs: List[InputSource]     # time/1PPS DPLL (Ch6) - TIME_ONLY, combo bus slave of Ch5
    warnings: List[str]
    errors: List[str]


def derive_plan(cfg: TimingConfig) -> DerivedPlan:
    """
    Derives the V6 DPLL Input Plan.

    V6 Architecture:
      - Channel 5 (Frequency): Locks ONLY to dedicated frequency sources.
          * FREQ_ONLY (e.g. SyncE, SMA with 10MHz)
          * If no FREQ_ONLY sources exist, Ch5 gets frequency via Combo Bus from Ch6.
      - Channel 6 (Time): Locks to any source providing 1PPS/Time.
          * TIME_AND_FREQ (e.g. GPS) - provides BOTH time and frequency to Ch6
          * TIME_ONLY (e.g. SMA with 1PPS)
      - Combo Bus direction is determined by apply_timing.py based on which channel
        has direct inputs.

    Logic:
      1. channel5_inputs = FREQ_ONLY only
      2. channel6_inputs = TIME_AND_FREQ + TIME_ONLY
      3. Sort both lists by priority (0=highest).
    """

    warnings: List[str] = []
    errors: List[str] = []

    ptp_role = PtpRole(cfg.ptp_role)

    # Build a flat list of InputSource from GPS, CM4, SyncE, and SMA inputs
    inputs: List[InputSource] = []

    # GPS
    if cfg.gps.present:
        # Default GPS priority=1 if not set
        prio = cfg.gps.priority if cfg.gps.priority is not None else 1
        role_enum = TimeFreqRole(cfg.gps.role)
        # Check explicit valid values
        if role_enum == TimeFreqRole.NONE:
            pass  # GPS present but role=NONE -> ignore
        else:
            inputs.append(InputSource(name="GPS", role=role_enum, priority=prio))

    # CM4
    if cfg.cm4.used_as_source:
        prio = cfg.cm4.priority if cfg.cm4.priority is not None else 2
        role_enum = TimeFreqRole(cfg.cm4.role)
        if role_enum != TimeFreqRole.NONE:
            inputs.append(InputSource(name="CM4", role=role_enum, priority=prio))

    # SyncE (Freq only)
    if cfg.synce.used_as_source:
        prio = cfg.synce.priority if cfg.synce.priority is not None else 3
        inputs.append(InputSource(name="SYNC_E", role=TimeFreqRole.FREQ_ONLY, priority=prio))

    # SMA Inputs
    for sma in cfg.smas:
        if sma.direction == SmaDirection.INPUT.value:
            # Must have priority
            if sma.priority is None:
                warnings.append(f"{sma.name} is INPUT but priority is not set. Ignoring.")
                continue
            
            # Must have role
            if sma.role is None:
                warnings.append(f"{sma.name} is INPUT but role is not set. Ignoring.")
                continue

            r = TimeFreqRole(sma.role)
            inputs.append(InputSource(name=sma.name, role=r, priority=sma.priority))


    # Setup source categories
    time_freq_sources = [s for s in inputs if s.role == TimeFreqRole.TIME_AND_FREQ]
    freq_only_sources = [s for s in inputs if s.role == TimeFreqRole.FREQ_ONLY]
    time_only_sources = [s for s in inputs if s.role == TimeFreqRole.TIME_ONLY]

    # V6 Logic: Populating Channel 5 (Freq) and Channel 6 (Time)
    channel5_inputs: List[InputSource] = []
    channel6_inputs: List[InputSource] = []

    # Channel 5 takes ONLY dedicated frequency sources (FREQ_ONLY)
    # TIME_AND_FREQ sources (like GPS) go to Channel 6 only;
    # Channel 5 gets frequency from Channel 6 via Combo Bus in that case.
    channel5_inputs.extend(freq_only_sources)
    channel5_inputs.sort(key=lambda s: s.priority)

    # Channel 6 takes anything with Time
    channel6_inputs.extend(time_freq_sources)
    channel6_inputs.extend(time_only_sources)
    channel6_inputs.sort(key=lambda s: s.priority)

    # Sanity checks
    
    # a) GM must have at least one time source (Ch6)
    if ptp_role == PtpRole.GM and not channel6_inputs:
        errors.append(
            "CM4 is configured as PTP GM, but there are no TIME or TIME_AND_FREQ sources "
            "for Channel 6 (Time/1PPS)."
        )

    # b) If we have Time sources but no Frequency sources, Ch5 is empty.
    #    This is OK - Ch5 will get frequency via Combo Bus from Ch6.
    if channel6_inputs and not channel5_inputs:
        warnings.append(
            "Channel 5 (Freq) has no direct inputs. It will get frequency "
            "via Combo Bus from Channel 6 (Time)."
        )

    # c) Check duplicate priorities per lists
    def find_duplicate_priorities(sources: List[InputSource], label: str):
        seen: Dict[int, List[str]] = {}
        for s in sources:
            seen.setdefault(s.priority, []).append(s.name)
        for prio, names in seen.items():
            if len(names) > 1:
                warnings.append(
                    f"{label}: multiple sources share priority {prio}: {', '.join(names)}."
                )

    find_duplicate_priorities(channel5_inputs, "Channel 5 (Freq) inputs")
    find_duplicate_priorities(channel6_inputs, "Channel 6 (Time) inputs")

    # d) SMA consistency checks + special SMA1 rules
    for sma in cfg.smas:
        if sma.direction == SmaDirection.OUTPUT.value:
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
                # V6: SMA3 is on Q9/Ch5 (Frequency only)
                if sma.name == "SMA3":
                    # Warn if user tries to set 1PPS, as it won't be phase-aligned
                    if sma.frequency_hz == 1:
                        warnings.append(
                            "SMA3 is driven by Channel 5 (Freq DPLL) and cannot provide "
                            "phase-aligned 1PPS. Use SMA1 for phase-aligned 1PPS."
                        )
                    # We no longer force 10MHz, so we don't warn about "ignored" config.
                    # But we can warn that it is integer-divided from DCO.
                    pass

                elif sma.frequency_hz is None:
                    warnings.append(
                        f"{sma.name}: OUTPUT selected but frequency_hz is not set."
                    )

            if sma.role is not None or sma.priority is not None:
                warnings.append(
                    f"{sma.name}: OUTPUT selected, but input role/priority are set. "
                    "They will be ignored."
                )

    return DerivedPlan(
        channel5_inputs=channel5_inputs,
        channel6_inputs=channel6_inputs,
        warnings=warnings,
        errors=errors,
    )


# ---------- SMA Mapping Helpers ----------

def hw_to_user_sma(hw_name: str) -> str:
    """
    Maps Hardware SMA name (Schematic/Front-Panel) to User SMA name (Rear-Panel).
    Mapping (V6):
      HW SMA1 <-> User SMA4
      HW SMA2 <-> User SMA3
      HW SMA3 <-> User SMA2
      HW SMA4 <-> User SMA1
    
    Formula: User# = 5 - HW#
    """
    try:
        hw_num = int(hw_name.replace("SMA", ""))
        user_num = 5 - hw_num
        return f"SMA{user_num}"
    except ValueError:
        return hw_name

def user_to_hw_sma(user_name: str) -> str:
    """
    Maps User SMA name (Rear-Panel) to Hardware SMA name.
    """
    try:
        user_num = int(user_name.replace("SMA", ""))
        hw_num = 5 - user_num
        return f"SMA{hw_num}"
    except ValueError:
        return user_name


def validate_config(cfg: TimingConfig) -> DerivedPlan:
    plan = derive_plan(cfg)
    ptp_role = PtpRole(cfg.ptp_role)

    # Track whether the user configured any FREQ_ONLY sources in the *config*
    configured_freq_only = []

    if cfg.synce.used_as_source:
        configured_freq_only.append("SYNC_E")

    for sma in cfg.smas:
        if sma.direction == SmaDirection.INPUT.value and sma.role == TimeFreqRole.FREQ_ONLY.value:
            configured_freq_only.append(sma.name)

    print("=== Derived 8A34004 DPLL Plan (V6 Architecture) ===")
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
    print("  Channel 5 (Frequency DPLL): Locks to FREQ_ONLY sources (SyncE, 10MHz).")
    print("  Channel 6 (Time/1PPS DPLL): Locks to TIME_AND_FREQ and TIME_ONLY sources.")
    if plan.channel5_inputs:
        print("  Combo Bus: Channel 6 slaves to Channel 5 (Freq Master).")
    elif plan.channel6_inputs:
        print("  Combo Bus: Channel 5 slaves to Channel 6 (Time Master, no direct freq source).")
    else:
        print("  Combo Bus: disabled (no sources).")

    # ---- Channel 5 list ----
    print("\nChannel 5 (Frequency) input priority list:")
    if plan.channel5_inputs:
        for s in plan.channel5_inputs:
            uname = s.name
            if s.name.startswith("SMA"):
                uname = f"{s.name} (User {hw_to_user_sma(s.name)})"
            print(f"  priority {s.priority}: {uname} ({s.role.value})")
    else:
        print("  (no frequency sources configured)")

    # ---- Channel 6 list ----
    print("\nChannel 6 (Time/1PPS) input priority list:")
    if plan.channel6_inputs:
        for s in plan.channel6_inputs:
            uname = s.name
            if s.name.startswith("SMA"):
                uname = f"{s.name} (User {hw_to_user_sma(s.name)})"
            print(f"  priority {s.priority}: {uname} ({s.role.value})")
    else:
        print("  (no time sources configured)")

    # ---- SMA summary (existing behavior) ----
    print("\nSMA configuration (showing Hardware Names [User Rear Panel Names]):")
    # Sort for display by User Number (Rear 1..4) implies HW 4..1
    # Let's just iterate the config list order, but adding User mappings
    
    # Actually, let's sort by User Name for approachability
    sorted_smas = sorted(cfg.smas, key=lambda s: hw_to_user_sma(s.name))
    
    for sma in sorted_smas:
        user_name = hw_to_user_sma(sma.name)
        label = f"{sma.name} [{user_name}]"
        
        if sma.direction == SmaDirection.INPUT.value:
            print(f"  {label}: INPUT, role={sma.role}, priority={sma.priority}, frequency={sma.frequency_hz}")
        elif sma.direction == SmaDirection.OUTPUT.value:
            if sma.name == "SMA1":
                if ptp_role == PtpRole.GM:
                    print(f"  {label}: OUTPUT, 1PPS (GM mode: Q11 forced to 1PPS for CM4)")
                else:
                    print(f"  {label}: OUTPUT, frequency={sma.frequency_hz} Hz (via Q11/Ch6, phase-aligned)")
            elif sma.name == "SMA2":
                print(f"  {label}: OUTPUT, frequency={sma.frequency_hz} Hz (via Q10)")
            elif sma.name == "SMA3":
                freq = sma.frequency_hz if sma.frequency_hz is not None else 10_000_000
                DCO_HZ = 500_000_000
                divider = round(DCO_HZ / freq)
                if divider < 1: divider = 1
                actual_hz = DCO_HZ / divider
                error_ppm = ((actual_hz - freq) / freq) * 1e6 if freq > 0 else 0
                if abs(error_ppm) > 0.001:
                    print(f"  {label}: OUTPUT, frequency={freq} Hz (via Q9/Ch5, output divider={divider}, actual={actual_hz:.3f} Hz, error={error_ppm:+.3f} ppm)")
                else:
                    print(f"  {label}: OUTPUT, frequency={freq} Hz (via Q9/Ch5, output divider={divider})")
            else:
                print(f"  {label}: OUTPUT, frequency={sma.frequency_hz} Hz")
        else:
            print(f"  {label}: UNUSED")

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
    print("Important (V6 Architecture):")
    print("  • The DPLL has separate TIME and FREQUENCY priority lists:")
    print("      - FREQUENCY domain: Channel 5 (SyncE / Frequency inputs)")
    print("      - TIME domain: Channel 6 (1PPS / TOC inputs)")
    print("  • Channel 6 is typically frequency-slaved to Channel 5.")
    print("  • Each reference gets a single 'priority' number;")
    print("    that priority is applied to whichever domain(s) it participates in,")
    print("    based on its ROLE:")
    print("      - TIME_ONLY       → affects time priority (Ch6) only")
    print("      - FREQ_ONLY       → affects frequency priority (Ch5) only")
    print("      - TIME_AND_FREQ   → affects both (Ch5 and Ch6)")
    print()
    print("SMA Mapping:")
    print("  The wizard uses User/Rear-Panel numbering (SMA1..SMA4).")
    print("  (Advanced: See README for mapping to schematic names)")
    print()
    print()
    print("SMA capabilities:")
    print("  - SMA1: Input-only.")
    print("  - SMA2: Fixed 10MHz Output (V6) / or Input.")
    print("  - SMA3: Input or Output (Arbitrary Freq via Q10).")
    print("  - SMA4: Input or Output (1PPS Phase Aligned). Special constraints if GM.")
    print()

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
    used_time_priorities: set = set()   # Ch6 (time domain) assigned priorities
    used_freq_priorities: set = set()   # Ch5 (freq domain) assigned priorities

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
            # GPS TIME_AND_FREQ/TIME_ONLY → goes to Ch6 (time domain)
            next_prio = 0
            while next_prio in used_time_priorities:
                next_prio += 1
            pr = input(f"GPS time-domain priority (0 = highest) [{next_prio}]: ").strip() or str(next_prio)
            try:
                gps_priority = int(pr)
            except ValueError:
                gps_priority = next_prio
            used_time_priorities.add(gps_priority)
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
        # CM4 TIME_ONLY/TIME_AND_FREQ → goes to Ch6 (time domain)
        next_prio = 0
        while next_prio in used_time_priorities:
            next_prio += 1
        pr = input(f"CM4 PPS time-domain priority (0 = highest) [{next_prio}]: ").strip() or str(next_prio)
        try:
            cm4_prio = int(pr)
        except ValueError:
            cm4_prio = next_prio
        used_time_priorities.add(cm4_prio)
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
        # SyncE FREQ_ONLY → goes to Ch5 (freq domain)
        next_prio = 0
        while next_prio in used_freq_priorities:
            next_prio += 1
        pr = input(f"SyncE freq-domain priority (0 = highest) [{next_prio}]: ").strip() or str(next_prio)
        try:
            synce_prio = int(pr)
        except ValueError:
            synce_prio = next_prio
        used_freq_priorities.add(synce_prio)

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
    print("\nStep 5: SMA configuration (User/Rear-Panel SMA1..SMA4)")
    
    smas: List[SmaConfig] = []
    
    # User SMA 1..4 (Rear view)
    for i in range(1, 5):
        user_name = f"SMA{i}"
        hw_name = user_to_hw_sma(user_name)
        
        print(f"\nConfiguring {user_name}:")
        
        # Branch based on HW capabilities
        if hw_name == "SMA1":
            # User SMA4
            if role_str == "GM":
                print(f"  Note: CM4 is PTP GM, so {user_name} cannot be used as an OUTPUT.")
                print("        (it shares the CM4 1PPS path). You may use it as INPUT or UNUSED.")
                prompt = "  Direction (INPUT/UNUSED) [UNUSED]: "
                allowed = {"INPUT", "UNUSED"}
            else:
                print(f"  Note: {user_name} supports Input or Output (1PPS Phase Aligned).")
                prompt = "  Direction (INPUT/OUTPUT/UNUSED) [UNUSED]: "
                allowed = {"INPUT", "OUTPUT", "UNUSED"}
                
        elif hw_name == "SMA4":
            # User SMA1
            print(f"  Note: {user_name} is INPUT-ONLY.")
            prompt = "  Direction (INPUT/UNUSED) [UNUSED]: "
            allowed = {"INPUT", "UNUSED"}
            
        elif hw_name == "SMA3":
            # User SMA2
            print(f"  Note: {user_name} is Input or Output.")
            print("        In V6, Output is Fixed 10MHz (SyncE Freq).")
            prompt = "  Direction (INPUT/OUTPUT/UNUSED) [UNUSED]: "
            allowed = {"INPUT", "OUTPUT", "UNUSED"}
            
        else: # SMA2 (User SMA3)
            print(f"  Note: {user_name} is Input or Output (Configurable Freq).")
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
            # Pick the right domain set based on role
            if r == "FREQ_ONLY":
                prio_set = used_freq_priorities
                domain_label = "freq-domain"
                default_freq = 10000000
            else:
                prio_set = used_time_priorities
                domain_label = "time-domain"
                default_freq = 1
                
            next_prio = 0
            while next_prio in prio_set:
                next_prio += 1
            pr = input(f"  {domain_label.capitalize()} priority (0 = highest) [{next_prio}]: ").strip() or str(next_prio)
            try:
                prio = int(pr)
            except ValueError:
                prio = next_prio
            prio_set.add(prio)
            
            # Prompt for Input Frequency
            if r in ("FREQ_ONLY", "TIME_AND_FREQ"):
                # Usually want to know nominal freq for lock
                prompt_freq = f"  Input Frequency in Hz [{default_freq}]: "
                f_str = input(prompt_freq).strip() or str(default_freq)
                try:
                    in_freq = int(f_str)
                except ValueError:
                    in_freq = default_freq
            else:
                # TIME_ONLY usually 1PPS
                in_freq = 1

            smas.append(
                SmaConfig(
                    name=hw_name,   # STORE AS HW NAME
                    direction=SmaDirection.INPUT.value,
                    role=r,
                    priority=prio,
                    frequency_hz=in_freq,
                )
            )
        elif d == "OUTPUT":
            if hw_name == "SMA1":
                # User SMA4
                # In GM mode, this is blocked above. In Client/None, allow freq.
                print("  SMA1 (HW) is driven by Channel 6 (Time).")
                print("  Typically 1PPS, but you can request other frequencies.")
                
                while True:
                    f_str = input("  Desired output frequency in Hz (1 to 250000000) [1]: ").strip() or "1"
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
                        name=hw_name,
                        direction=SmaDirection.OUTPUT.value,
                        role=None,
                        priority=None,
                        frequency_hz=freq,
                    )
                )

            elif hw_name == "SMA3":
                # User SMA2
                # Driven by Q9 (Channel 5). 
                print("  SMA3 (HW) is driven by Channel 5 (SyncE/Freq).")
                print("  Note: Frequency is derived via integer output divider from DCO.")
                print("        Exact frequency references (e.g. 10MHz) are best.")
                print("        Arbitrary frequencies may have PPM error.")
                
                DCO_HZ = 500_000_000  # Must match DCO_FREQ_HZ in apply_timing.py
                
                while True:
                    f_str = input("  Desired output frequency in Hz [10000000]: ").strip() or "10000000"
                    try:
                        freq = int(f_str)
                    except ValueError:
                        print("    Please enter an integer.")
                        continue
                    if not (1 <= freq <= 250_000_000):
                        print("    Frequency must be between 1 and 250000000 Hz.")
                        continue
                    
                    # Show divider resolution warning
                    divider = round(DCO_HZ / freq)
                    if divider < 1: divider = 1
                    actual_hz = DCO_HZ / divider
                    error_ppm = ((actual_hz - freq) / freq) * 1e6
                    
                    if abs(error_ppm) > 0.001:
                        print(f"\n    *** Divider Resolution Warning ***")
                        print(f"    DCO = {DCO_HZ} Hz, Integer Divider = {divider}")
                        print(f"    Requested: {freq} Hz")
                        print(f"    Actual:    {actual_hz:.6f} Hz")
                        print(f"    Error:     {error_ppm:+.3f} ppm")
                        ok = input("    Accept this frequency? (y/n) [y]: ").strip().lower() or "y"
                        if ok != "y":
                            continue
                    else:
                        print(f"    Exact match: DCO / {divider} = {actual_hz:.0f} Hz")
                    break

                smas.append(
                    SmaConfig(
                        name=hw_name,
                        direction=SmaDirection.OUTPUT.value,
                        role=None,
                        priority=None,
                        frequency_hz=freq,
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
                        name=hw_name,
                        direction=SmaDirection.OUTPUT.value,
                        role=None,
                        priority=None,
                        frequency_hz=freq,
                    )
                )
        else:
            smas.append(
                SmaConfig(
                    name=hw_name,
                    direction=SmaDirection.UNUSED.value,
                    role=None,
                    priority=None,
                    frequency_hz=None,
                )
            )
    
    # Sort SMAs by HW name to match 1..4 order in JSON (not strictly required but cleaner)
    smas.sort(key=lambda s: s.name)

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
    print("to see how this maps into DPLL channel priorities (ch5 & ch6) and logical User SMA names.")



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
        print("Channel 5 inputs (Freq):", [(s.name, s.role.value, s.priority) for s in plan.channel5_inputs])
        print("Channel 6 inputs (Time):", [(s.name, s.role.value, s.priority) for s in plan.channel6_inputs])
        if plan.warnings:
            print("Warnings:", plan.warnings)
        if plan.errors:
            print("Errors:", plan.errors)


if __name__ == "__main__":
    main()
