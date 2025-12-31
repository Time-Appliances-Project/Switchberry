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
  - Validator produces a derived “plan” of what feeds Channel 2 vs Channels 5/6

- `software/clockmatrix/apply_timing.py`  
  Applies a `config.py` JSON to the live system:
  - Configures Switchberry **board multiplexers** via `gpioset`
  - Programs the DPLL input frequencies + channel priority lists via `dplltool`
  - Configures DPLL outputs (OUT3/OUT4) used to drive SMA2/SMA3

- `software/clockmatrix/fastlock_1pps_fix.sh`  
  Simple daemon monitor that checks DPLL lock state and **forces relock** when needed:
  - Channels **5 & 6** (1PPS “time” channels) can fail to fastlock after unlock events
  - Channel **2** (SyncE “frequency” channel) can also get into unstable states
  - This script detects those conditions and applies a “reset pulse” (FREERUN → NORMAL)

---

## Background (what this subsystem is doing)

Switchberry uses the 8A34004 to combine and distribute two timing domains:

- **SyncE (frequency / syntonization)**  
  Provides a stable frequency reference recovered from the network (no packet jitter).

- **PTP/GNSS (time / phase, typically via 1PPS)**  
  Provides phase/time alignment (grandmaster/client use-cases, PPS distribution).

A DPLL lets you:
- lock to a **frequency** input (e.g., SyncE recovered clock),
- lock to one or more **1PPS** references for phase/time alignment,
- switch between sources by priority,
- maintain stability via filtering/holdover (depending on configuration).

On Switchberry, the typical model is:
- **Channel 2** ≈ “frequency” domain (e.g., SyncE)
- **Channels 5 & 6** ≈ “time/phase” domain (1PPS), often coupled to the frequency domain via internal routing (combo bus) in your TCS.

> The exact 8A34004 channel topology (combo bus / followers) comes from your Timing Commander configuration.  
> The tools here assume the “Switchberry baseline” where ch2 is frequency-oriented and ch5/6 are time-oriented.

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
./dplltool --flash-hex SwitchberryV5_8a34004_eeprom.hex
```

- Apply a `.tcs` live config:
```bash
./dplltool --tcs-apply SwitchberryV5_8a34004_live.tcs
```

- Apply a “programming file” text export (register writes):
```bash
./dplltool --prog-file Test_FromScratch_Programming_11-26-2025.txt
```

#### Monitor-friendly state/phase helpers

These are intentionally script-friendly (single token / single number outputs):

- Lock state:
```bash
./dplltool get_state 2
# -> LOCKED | LOCKACQ | LOCKREC | FREERUN | HOLDOVER | DISABLED | UNKNOWN
```

- State-change sticky bit (flap detection):
```bash
./dplltool get_statechg_sticky 2
# -> 0 or 1
./dplltool clear_statechg_sticky 2
```

- Phase status (prints a float in seconds):
```bash
./dplltool get_phase 5
# -> e.g. 1.234567890123e-09
```

- Force operating state (used by the monitor script):
```bash
./dplltool set_oper_state 5 FREERUN
./dplltool set_oper_state 5 NORMAL
./dplltool set_oper_state 5 HOLDOVER
```

#### High-level control commands (used by `apply_timing.py`)

These are the main “operational knobs” used at boot/config time:

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

- Configure OUT3 and OUT4 frequencies together:
```bash
./dplltool set-output-freq <freq3_hz> <freq4_hz>
```

Notes:
- Output frequencies may be integer or floating-point (e.g. `10e6`, `10.000001e6`).
- OUT2 is treated as “special” on Switchberry (typically 1PPS); `apply_timing.py` manages OUT2 routing via GPIO muxes.

#### Advanced debug helpers

- Output phase adjust (s32 units per ClockMatrix register definition):
```bash
./dplltool out-phase-adj-get <out_index>
./dplltool out-phase-adj-set <out_index> <adj_s32>
```

- WR frequency word read/write (s42 fixed-point; also supports “ppb” convenience):
```bash
./dplltool wr-freq-get <dpll_index>
./dplltool wr-freq-set-word <dpll_index> <word_s42>
./dplltool wr-freq-set-ppb <dpll_index> <ppb_float>
```

#### `--set-out2-dest` note
The CLI help mentions:
```bash
./dplltool --set-out2-dest <CM4_PPS|SMA1_PATH>
```
…but in the current `dpll_utility.c` this action is a stub/no-op. OUT2 routing is handled by **board mux GPIOs** in `apply_timing.py`.

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

### Roles and priorities

Roles:
- `TIME_ONLY` – typically **1PPS**
- `FREQ_ONLY` – continuous frequency reference (e.g., **10 MHz** or **SyncE recovered** clock)
- `TIME_AND_FREQ` – a reference treated as both time and frequency

Priorities:
- Integer, **1 = highest**
- Priorities are per “domain” (frequency vs time). `derive_plan()` will warn on duplicates.

### Derived plan (how it maps into DPLL channels)

`derive_plan()` computes two input lists:
- `channel2_inputs` – intended for **Channel 2** (frequency)
- `time_channel_inputs` – intended for **Channels 5 & 6** (time/phase)

Key behavior from the code:
- If **any `TIME_AND_FREQ`** sources exist:
  - Channel 2 inputs are **disabled** in the derived plan (ch2 is expected to follow via internal routing in your ClockMatrix setup).
  - `FREQ_ONLY` sources will be **ignored** for Channel 2 (warning emitted).
- If time-only references exist but no frequency reference exists, a warning is emitted because Channel 2 may be free-running depending on your TCS.

### SMA rules (from the wizard/validator)

- SMA1 can be `INPUT`, `OUTPUT`, or `UNUSED`, but:
  - **SMA1 OUTPUT is special:** it only supports **1PPS** behavior in this intent layer.
  - **SMA1 OUTPUT is not allowed when CM4 is PTP GM**, because that path is reserved for DPLL→CM4.
- **SMA4 is input-only** (no output path).
- SMA `OUTPUT` requires an output frequency (wizard enforces `1..250000000` Hz).
- SMA `INPUT` requires `role` + `priority`.

### Config JSON schema (overview)

The JSON is written by the wizard and loaded by `apply_timing.py`. Conceptually:

- `ptp_role`: `"GM" | "CLIENT" | "NONE"`
- `gps`: `{ present: bool, role: "...", priority: int }`
- `cm4`: `{ used_as_source: bool, role: "...", priority: int }`
- `synce`: `{ used_as_source: bool, priority: int }`
- `smas`: list of four SMA objects:
  - `{ name: "SMA1".."SMA4", direction: "INPUT|OUTPUT|UNUSED", role, priority, frequency_hz }`

Example (illustrative):
```json
{
  "ptp_role": "CLIENT",
  "gps":  { "present": true,  "role": "TIME_ONLY", "priority": 1 },
  "cm4":  { "used_as_source": true, "role": "TIME_ONLY", "priority": 2 },
  "synce":{ "used_as_source": true, "priority": 1 },
  "smas": [
    { "name": "SMA1", "direction": "UNUSED", "role": null, "priority": null, "frequency_hz": null },
    { "name": "SMA2", "direction": "OUTPUT", "role": null, "priority": null, "frequency_hz": 10000000 },
    { "name": "SMA3", "direction": "OUTPUT", "role": null, "priority": null, "frequency_hz": 1 },
    { "name": "SMA4", "direction": "INPUT",  "role": "FREQ_ONLY", "priority": 2, "frequency_hz": null }
  ]
}
```

---

## Applying a config: `apply_timing.py`

`apply_timing.py` loads the JSON config, derives the plan, then applies it in three steps:

1) **Configure board multiplexers** using `gpioset`  
2) **Configure DPLL inputs and channel priorities** using `dplltool`  
3) **Configure DPLL outputs** (OUT3/OUT4) using `dplltool`

### Usage

Default config path is `/etc/dpll-config.json`:
```bash
sudo python3 apply_timing.py
```

Specify config path:
```bash
sudo python3 apply_timing.py --config ./dpll-config.json
```

Dry run (prints commands without executing):
```bash
sudo python3 apply_timing.py --config ./dpll-config.json --dry-run
```

### Source → DPLL input mapping (Switchberry wiring)

From `apply_timing.py`:

Board-level “source names” map to **logical inputs (1..4)**:
- `SYNC_E` → 1
- `SMA1`   → 1  (shares path with SyncE; cannot be used simultaneously)
- `CM4`    → 2
- `SMA2`   → 2
- `GPS`    → 3
- `SMA3`   → 3
- `SMA4`   → 4

Logical inputs map to **ClockMatrix Input instances** used by `dplltool`:
- logical 1 → physical input instance **0**
- logical 2 → physical input instance **8**
- logical 3 → physical input instance **1**
- logical 4 → physical input instance **9**

> This matches the “differential pair” style inputs used in the current Switchberry TCS.

If two sources would claim the same logical input, `apply_timing.py` raises a conflict error (example given in code: `SYNC_E` and `SMA1` simultaneously).

### Nominal input frequency assumptions

`apply_timing.py` sets nominal input frequencies based on role/name:
- `TIME_ONLY` / `TIME_AND_FREQ` → **1 Hz** (1PPS)
- `SYNC_E` → **25,000,000 Hz** (assumed; adjust if needed)
- `SMA*` `FREQ_ONLY` → **10,000,000 Hz** (assumed; adjust if needed)

These are hard-coded defaults in `nominal_input_freq_hz()`.

### Board mux GPIO mapping (as implemented)

`apply_timing.py` uses `gpioset gpiochip2 ...` for mux control.

**CM4 PPS routing**

**M.2 PPS routing**

**SMA front-end muxes**

### DPLL channel programming behavior

- Inputs used by the derived plan are:
  - assigned a nominal frequency (`dplltool set-input-freq`)
  - attached to a channel priority list (`dplltool set-chan-input`)
  - enabled (`dplltool set-input-enable enable`)
- Unused inputs are explicitly disabled.

Priority conversion:
- Config uses **1 = highest**
- Hardware registers use **0 = highest**
- `apply_timing.py` calls `priority_hw = priority - 1`

### Output programming behavior

- OUT2 is treated as fixed “1PPS infrastructure” and routed by muxes.
- OUT3 and OUT4 are driven based on SMA output requests:
  - OUT3 → SMA2
  - OUT4 → SMA3
- `dplltool set-output-freq <out3> <out4>` sets both together.

If only one of SMA2/SMA3 is used as OUTPUT, the other is filled with a default placeholder (currently **1 Hz**) so the combined setter can still be called.

---

## Workaround daemon: `fastlock_1pps_fix.sh`

`fastlock_1pps_fix.sh` is a **monitor loop** intended to run continuously (e.g., systemd service).
It exists because experimentally:
- ClockMatrix “fastlock” behavior may fail when **channels 5/6** unlock (1PPS channels)
- **Channel 2** can sometimes get into unstable states during SyncE disturbances

The script polls:
- `dplltool get_state <chan>`
- `dplltool get_statechg_sticky <chan>`
- `dplltool clear_statechg_sticky <chan>`

And applies a “reset pulse” when needed:
- `dplltool set_oper_state <chan> FREERUN`
- sleep
- `dplltool set_oper_state <chan> NORMAL`

### Default channels monitored
From the script:
- CH2 = `2`  (SyncE/frequency channel)
- GPS channels = `5` and `6` (1PPS/time channels)

### What it detects

Two main failure patterns:
1) **Flapping**: too many state-change sticky transitions within a window  
2) **Unlocked too long**: channel remains not-LOCKED past a threshold

CH2 special case:
- If CH2 is unlocked *but* has had **no state changes** for a while, it’s treated as a valid “no input present” condition and the script does not keep resetting it.

Combo-bus step handling:
- When CH2 becomes **stably locked**, the script can optionally reset or aggressively monitor CH5/6 because a frequency step can disrupt them.

Key tuning knobs are at the top of the script (`POLL_SEC`, `*_UNLOCK_SEC`, `*_FLAP_*`, cooldowns, pulses, etc.).

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
  sudo ./dplltool get_state 2
  sudo ./dplltool get_state 5
  sudo ./dplltool get_state 6
  ```
- If channels 5/6 won’t relock after a disturbance, run the monitor script and watch its logs:
  ```bash
  sudo ./fastlock_1pps_fix.sh
  ```

---