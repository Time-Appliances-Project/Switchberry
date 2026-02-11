# Switchberry ClockMatrix Utilities (Renesas 8A34004)

This directory contains the **Switchberry ClockMatrix (Renesas 8A34004) software subsystem**.

It’s split into:
- a **low-level** SPI/register utility (`dplltool`) used to program and interrogate the 8A34004, and
- a **high-level** configuration layer (`config.py` + `apply_timing.py`) that lets you describe *intent* (sources, priorities, frequencies, SMA routing) and apply it to the board.

There is also a small **monitor/repair daemon** (`fastlock_1pps_fix.sh`) that works around observed ClockMatrix “stuck/unlocked” behavior by forcing targeted relocks.

---

## Directory layout

- `software/clockmatrix/dpll/`  
  Low-level C utilities that build into **`dplltool`**:
  - SPI register read/write
  - Apply Timing Commander exports (TCS / programming TXT)
  - Configure inputs, priorities, and outputs
  - Query lock state, sticky state-change, and phase
  - Advanced debug helpers (output phase adjust, WR frequency word read/write)

- `software/clockmatrix/config.py`  
  High-level **config generator + validator**.
  - Interactive wizard creates a JSON file describing timing intent
  - Validator produces a derived “plan” of what feeds Channel 5 vs Channel 6

- `software/clockmatrix/apply_timing.py`  
  Applies a `config.py` JSON to the live system:
  - Configures Switchberry **board multiplexers** via `gpioset`
  - Programs the DPLL input frequencies + channel priority lists via `dplltool`
  - Configures DPLL outputs (Q10/Q11) used to drive SMA2/SMA1

- `software/clockmatrix/dpll/fastlock_1pps_fix.sh`  
  **DPLL monitor and automatic relocker daemon.**  
  Runs as `switchberry-dpll-monitor.service` and continuously monitors DPLL lock health.

  **Why it exists:** The 8A34004 DPLL can get "stuck" in non-optimal states after input disturbances, power-up transients, or combo-bus frequency steps. Additionally, the hardware lock detector can falsely report LOCKED while the actual phase offset is well above the configured threshold (e.g. 380ns when the threshold is 100ns).

  **Three decision triggers (Channel 6 only — Channel 5 is observe-only):**
  1. **State flapping** — Channel state changes too many times within a sliding window → force relock
  2. **Stuck unlocked** — Channel stays in LOCKREC/LOCKACQ too long without recovering → force relock
  3. **Phase offset too large** — Channel reports LOCKED but `dplltool get-phase` shows the actual offset exceeds a trigger threshold (default 250ns) for a sustained period (default 20s) → force relock. Uses hysteresis: the timer only clears when phase drops below a lower clear threshold (default 150ns), preventing timer resets from minor dips around the boundary.

  **Relock mechanism:** Cycles the channel `FREERUN → (wait) → NORMAL` to force the DPLL to re-acquire lock from scratch.

  **Status file** (`/tmp/switchberry-clockmatrix.status`): 3-line file that other services (ts2phc, ptp4l guard scripts) can check:
  - `OK` = system is in a valid steady state, not intervening
  - `NOT_OK` = actively intervening, downstream consumers should pause

  **Key configuration knobs** (at top of script):
  | Variable | Default | Purpose |
  |----------|---------|---------|
  | `POLL_SEC` | 0.2s | Main loop poll interval |
  | `STARTUP_AGGRESSIVE_SEC` | 60s | Use tighter thresholds after boot |
  | `PHASE_THRESHOLD_SEC` | 250ns | Phase offset trigger threshold |
  | `PHASE_CLEAR_THRESHOLD_SEC` | 150ns | Phase must drop below this to clear timer (hysteresis) |
  | `PHASE_EXCEED_SEC` | 20s | How long phase must exceed trigger before relock |
  | `FREQ_LOCK_EVENT_GPS_ACTION` | 1 | Force-reset GPS channels when Ch5 stably locks |

---

## Background: System Architecture (V6)

Switchberry uses the 8A34004 to combine and distribute two timing domains:

**1. Frequency Domain (Channel 5)**
   - Locks to stable frequency sources (SyncE, 10MHz).
   - Provides the "heartbeat" stability for the system.
   - Drives **SMA3** (Fixed 10MHz) and **SMA2** (Configurable Freq).

**2. Time Domain (Channel 6)**
   - Locks to 1PPS phase sources (GPS, SMA1).
   - Slaves its frequency from Channel 5 via the internal **Combo Bus**.
   - Drives **SMA1** and **CM4 1PPS** (Phase aligned).

### Architecture Diagram

```mermaid
graph TD
    subgraph "Inputs"
        SyncE[SyncE / 10MHz]
        GPS[GPS / 1PPS]
        SMA1_IN[SMA1 Input]
        CM4_IN[CM4 1PPS Input]
    end

    subgraph "8A34004 DPLL"
        direction TB
        subgraph "Freq Domain"
            Ch5[Channel 5: FREQUENCY]
        end
        
        subgraph "Time Domain"
            Ch6[Channel 6: TIME / 1PPS]
        end
        
        Combo((Combo Bus))
    end

    subgraph "Outputs"
        Q9[Q9: SMA3 (Fixed 10MHz)]
        Q10[Q10: SMA2 (Freq Out)]
        Q11[Q11: SMA1 / CM4 (1PPS)]
    end

    %% Input routing
    SyncE --> Ch5
    GPS -.->|Freq| Ch5
    SMA1_IN -->|Freq| Ch5
    
    GPS -->|Time| Ch6
    SMA1_IN -->|Time| Ch6
    CM4_IN -->|Time| Ch6

    %% Internal coupling
    Ch5 ==> Combo
    Combo ==>|Freq Stability| Ch6

    %% Output routing
    Ch5 --> Q9
    Ch5 --> Q10
    Ch6 --> Q11
```

### V6 Hardware Changes
In V6, the outputs are strictly separated to prevent frequency vs phase conflicts:
- **Q9 (Channel 5)**: Dedicated to **SMA3** for **frequency-only** output (SyncE aligned).
- **Q11 (Channel 6)**: Dedicated to **CM4 1PPS** and **SMA1** (phase aligned).
- **Q10**: Configurable frequency output for **SMA2**.

> **Note**: Channel 2 is considered legacy in this architecture and is not actively managed by `apply_timing.py`.

### Combo Bus Strategy & Independent Phase Locking (Feb 2026 Update)

A key architectural decision in the V6 software stack involves when to slave the Time DPLL (Ch6) to the Frequency DPLL (Ch5).

**The Problem:**
When a system has both a high-quality frequency reference (SyncE on Ch5) and a separate time reference (PTP/GPS on Ch6), essentially tracking two different masters, slaving Ch6 to Ch5 creates a conflict. The SyncE frequency may drift relative to the PTP time source (e.g. due to PTP NIC oscillator wander or network PDV). If Ch6 is forced to follow Ch5's frequency while trying to align phase to PTP 1PPS, the loop dynamics fight, causing phase drift ("walking away") or instability ("bouncing" between LOCKREC/LOCKED).

**The Solution:**
`apply_timing.py` implements a smart strategy:
1.  **If a Time Source (GPS/PTP/SMA 1PPS) is present:**
    *   **Channel 6 runs independently.** It locks both frequency and phase directly to its 1PPS input.
    *   This ensures Ch6 faithfully tracks the time source without interference from SyncE frequency drift.
    *   Channel 5 runs independently on SyncE (if present) or slaves to Ch6 (if no SyncE).
2.  **If ONLY Frequency Sources (SyncE/10MHz) are present:**
    *   **Channel 6 slaves to Channel 5.**
    *   Since there is no 1PPS input to track, Ch6 *must* derive its frequency from Ch5 to generate stable synthesized 1PPS output.

This approach provides the best stability for PTP clocks while maintaining SyncE frequency quality on the dedicated frequency outputs.

---

## Build `dplltool`

`dplltool` is built from `dpll_utility.c` plus supporting sources (SPI + Timing Commander table bindings).

Example build line (from the header in `dpll_utility.c`):
```bash
gcc -O2 -g -Wall -Wextra -o dplltool \
  dpll_utility.c linux_dpll.c tcs_dpll.c renesas_cm8a34001_tables.c
```

> Your repo layout may differ; use your project’s build system/Makefile if present.

---

## `dplltool` (low-level DPLL utility)

Run with no arguments to print the supported operations:
```bash
./dplltool
```

### Connection options

`dplltool` talks to the DPLL over Linux `spidev`.

- Default SPI device in code: `/dev/spidev7.0`
- Override with:
  - `--spidev /dev/spidevX.Y`, or
  - `--busnum <n> --csnum <m>` (builds `/dev/spidev<n>.<m>`)

Also supported:
- `--hz <freq>` (SPI clock, default 1,000,000)
- `--mode <0..3>` (SPI mode, default 0)

### Common actions

> `dplltool` requires you to specify **exactly one action** per invocation.

#### Register access
```bash
./dplltool --read  0xC024
./dplltool --write 0xCBE4 0x50
```

#### Apply Timing Commander exports

- Flash an EEPROM image:
```bash
./dplltool --flash-hex SwitchberryV6_8a34004_eeprom.hex
```

- Apply a `.tcs` live config:
```bash
./dplltool --tcs-apply SwitchberryV5_8a34004_live.tcs
```

#### Monitor-friendly state/phase helpers

- Lock state:
```bash
./dplltool get_state 5
# -> LOCKED | LOCKACQ | LOCKREC | FREERUN | HOLDOVER | DISABLED | UNKNOWN
```

- State-change sticky bit (flap detection):
```bash
./dplltool get_statechg_sticky 5
# -> 0 or 1
./dplltool clear_statechg_sticky 5
```

- Phase status (prints a float in seconds):
```bash
./dplltool get_phase 6
# -> e.g. 1.234567890123e-09
```

#### High-level control commands (used by `apply_timing.py`)

- Set nominal input frequency:
```bash
./dplltool set-input-freq <input_instance> <freq_hz>
```

- Enable/disable an input:
```bash
./dplltool set-input-enable <input_instance> <enable|disable>
```

- Configure a channel’s priority list entry:
```bash
./dplltool set-chan-input <chan> <input_instance> <priority> <enable|disable>
```

- Configure Q10/Q11 output frequencies:
```bash
./dplltool set-output-freq <q10_hz> <q11_hz>
```

- Set integer output divider:
```bash
./dplltool set-output-divider <output_idx> <divider>
```

- Configure combo bus slave:
```bash
./dplltool set-combo-slave <chan> <master_chan> <enable|disable>
```

---

## High-level configuration: `config.py`

`config.py` is an “intent config” layer. It produces a JSON file describing:
- what timing sources exist (GPS, CM4 PPS, SyncE, SMA inputs),
- what each source represents (time-only vs freq-only vs both),
- priority ordering, and
- SMA directions and output frequencies.

### Commands

Interactive wizard:
```bash
python3 config.py wizard -c dpll-config.json
```

Validate config and show the derived channel input lists:
```bash
python3 config.py validate -c dpll-config.json
```

Show derived plan only:
```bash
python3 config.py show-derived -c dpll-config.json
```

### Derived plan (how it maps into DPLL channels)

`derive_plan()` computes two input lists for the V6 architecture:

**Channel 5 (Frequency DPLL)**
- Locks to frequency sources:
  - `FREQ_ONLY` sources (SyncE, 10MHz SMA input)
  - `TIME_AND_FREQ` sources (e.g. GPS) treated as frequency reference

**Channel 6 (Time/1PPS DPLL)**
- Locks to time/1PPS sources:
  - `TIME_ONLY` sources (1PPS SMA input)
  - `TIME_AND_FREQ` sources (e.g. GPS, CM4 1PPS)

**Key behavior:**
- **Channel 6** (Time) typically slaves its frequency tracking from **Channel 5** via the internal Combo Bus.
- This means a single high-quality frequency source (like SyncE on Ch5) can discipline the 1PPS output phase (on Ch6).

### SMA rules (User/Rear Panel View)

The configuration tools use **User SMA Numbering (Rear Panel)**.
*(Internally, this maps to Hardware SMAs: User 1=HW4, User 2=HW3, User 3=HW2, User 4=HW1)*

- **User SMA1** (Hardware SMA4) is **input-only** (no output path).
- **User SMA4** (Hardware SMA1) output shares the same path as the CM4 1PPS:
  - **User SMA4 OUTPUT is special:** it only supports **1PPS** behavior.
  - **User SMA4 OUTPUT is not allowed when CM4 is PTP GM**, because that path is reserved for DPLL→CM4.
- **User SMA2** (Hardware SMA3) is fixed 10MHz Frequency Output (V6) or Frequency Input.
- **User SMA3** (Hardware SMA2) is Configurable Frequency Output or Input.

---

## Applying a config: `apply_timing.py`

`apply_timing.py` loads the JSON config, derives the plan, then applies it in three steps:

1) **Configure board multiplexers** using `gpioset`  
2) **Configure DPLL inputs and channel priorities** using `dplltool`  
3) **Configure DPLL outputs** (Q10/Q11) using `dplltool`

### Usage

Default config path is `/etc/dpll-config.json`:
```bash
sudo python3 apply_timing.py
```

### Source → DPLL input mapping

Board-level “source names” map to **logical inputs (1..4)**:
- `SYNC_E` / `SMA1` → **1**
- `CM4` / `SMA2`    → **2**
- `GPS` / `SMA3`    → **3**
- `SMA4`            → **4**

Logical inputs map to **ClockMatrix Physical Inputs**:
- logical 1 → physical **0**
- logical 2 → physical **8**
- logical 3 → physical **1**
- logical 4 → physical **9**

If two sources would claim the same logical input, `apply_timing.py` raises a conflict error.

---

## Workaround daemon: `fastlock_1pps_fix.sh`

`fastlock_1pps_fix.sh` is a **monitor loop** intended to run continuously.
It exists because experimentally:
- ClockMatrix “fastlock” behavior may fail when **channel 6** (Time/1PPS) unlocks.
- **Channel 5** (Frequency) can sometimes get into unstable states during SyncE disturbances.

The script polls:
- `dplltool get_state <chan>`

And applies a “reset pulse” when needed:
- `dplltool set_oper_state <chan> FREERUN`
- sleep
- `dplltool set_oper_state <chan> NORMAL`

### Default channels monitored
- FREQ_CH = `5`  (Frequency)
- GPS_CHANS = `6` (Time/1PPS)

### Run it
```bash
sudo ./fastlock_1pps_fix.sh
```

---

## Troubleshooting tips

- Verify `dplltool` can talk to the chip:
  ```bash
  sudo ./dplltool --read 0xC024
  ```
- Check channel lock state:
  ```bash
  sudo ./dplltool get_state 5  # Frequency
  sudo ./dplltool get_state 6  # Time
  ```
- If channels won’t relock after a disturbance, run the monitor script and watch its logs:
  ```bash
  sudo ./fastlock_1pps_fix.sh
  ```