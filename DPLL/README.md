# Switchberry 8A34004 (ClockMatrix) Configuration

This directory documents the **Renesas 8A34004 ClockMatrix DPLL** configuration used on **Switchberry**, and provides the configuration files needed to reproduce or modify it.

Switchberry uses the 8A34004 as the **central timing hub**: it can lock to network-derived frequency (SyncE), lock to time/phase references (PPS from GNSS or PTP), and synthesize **phase-aligned 1PPS** and **arbitrary frequency outputs** for distribution to the rest of the system (including the Ethernet switch).

---

## What’s in this directory

- **Timing Commander project (`.tcs`)**  
  Import into *Renesas Timing Commander* to inspect and edit the personality/configuration.

- **Programming file** (for runtime loading)  
  Used by `dplltool` (see the repo `Software/` directory) or directly on Switchberry to program the running configuration of the 8A34004.

> Tip: Timing Commander is the easiest way to explore what’s possible, change muxing, adjust priorities, tweak bandwidths, and regenerate an updated programming file.

---

## Background: why a DPLL matters for SyncE + PTP

Timing in packet networks often splits into two complementary problems:

### SyncE (frequency / syntonization)
**Synchronous Ethernet (SyncE)** distributes a *frequency reference* over the physical Ethernet layer.  
If you recover SyncE, your local oscillator can run at the **same frequency** as the network, with good wander/jitter behavior — but SyncE alone doesn’t tell you “what time is it” or provide absolute phase/time.

### PTP (time / phase)
**PTP (IEEE 1588)** distributes *time and phase* over packets.  
A PTP client can recover a 1PPS (and time-of-day) aligned to a grandmaster, but packet paths can have jitter and asymmetry. Many systems pair PTP with a clean frequency source (often SyncE) for best performance and stability.

### Where the 8A34004 fits
The 8A34004 is a “timing glue” device: it can
- **lock to frequency** inputs (e.g., recovered SyncE)
- **align and generate phase** outputs (e.g., 1PPS)
- **filter jitter**, manage **holdover**, and **switch references** based on priority/health
- combine frequency + phase sources so you can recover “best of both worlds” from the network

On Switchberry, this enables:
- SyncE-only operation (frequency recovered from the switch)
- PPS-only operation (GNSS or external 1PPS drives phase)
- combined SyncE + PPS operation (frequency from SyncE, phase/time from PPS)
- “network recovered” time+frequency (PTP-derived PPS + SyncE frequency)

---

## Switchberry timing architecture (high level)

Switchberry uses the 8A34004 in a “combo” style arrangement:

- **System DPLL** generates the **local frequency reference** from the on-board **TCXO**
- **Channel 2** is used as the **primary frequency-tracking channel** (typically locks to SyncE)
- **Channels 5 & 6** act as “satellite” channels that can lock to one or more **PPS** sources to produce **phase-aligned outputs**, while tracking the chosen frequency reference

A key concept here is the **Combo Bus** inside the chip:
- Channel 2 can lock to a frequency source (e.g., SyncE)
- Channel 2 exports frequency tracking internally via the combo bus
- Channels 5 & 6 can then use that shared frequency basis while aligning phase to PPS

This lets Switchberry “mix and match”:
- **frequency** from SyncE (stable syntonization)
- **phase/time** from GNSS PPS or PTP-recovered PPS
- generate outputs that are frequency-accurate, phase-aligned, and stable through outages (holdover)

---

## Baseline configuration summary

### DPLL channels used
- **Channel 2**: frequency master (no direct output driving in this design)
- **Channel 5**: phase/PPS + frequency combo follower (drives outputs)
- **Channel 6**: phase/PPS + frequency combo follower (drives outputs)

> Note: Channel numbering reflects the ClockMatrix / Timing Commander channel model.

### Default locking intent
- **System DPLL**: free-runs / stabilizes from **local TCXO** (local reference)
- **Channel 2**:
  - tries to lock to **CLK0** using a **SyncE-oriented profile**
  - acts as the **Combo Bus** source/follower as configured (frequency distribution to other channels)
- **Channels 5 & 6**:
  - try to lock to **CLK1** using a **GNSS upconvert**-style profile
  - follow frequency via combo relationships (Channel 2 and/or System DPLL) and align phase to PPS

---

## Clock input mapping (baseline)

> The SMA ports are muxed. The “or” options below reflect the board routing choices.

| Input | Routed From | Default | Typical Use |
|------:|-------------|---------|-------------|
| **CLK0** | SMA1 **or** SyncE input (from KSZ9567 recovered clock) | **25 MHz** | Primary **frequency** reference (SyncE) |
| **CLK8** | SMA2 **or** CM4 PPS output | **1PPS** | Optional PPS source (from CM4 / PTP) |
| **CLK1** | SMA3 **or** OCP M.2 GNSS PPS output | **1PPS** | Primary **phase/time** reference (GNSS PPS) |
| **CLK9** | SMA4 **or** KSZ9567 PPS output | **1PPS** | Optional PPS source (from switch/PTP path) |
| **GPIO3** | Loopback from Q11 | — | Debug/measurement (usually not needed) |

---

## Clock output mapping (baseline)

| Output | Routed To | Default | Purpose |
|-------:|-----------|---------|---------|
| **Q8** | KSZ9567 + KSZ9031 core clock | **25 MHz** | Ethernet silicon reference clocking |
| **Q9** | SMA1 **or** CM4 PPS input | **1PPS** | Deliver/receive PPS between CM4 and timing fabric |
| **Q10** | SMA2 **or** KSZ9567 PPS input | **1PPS** | Deliver/receive PPS between switch/PTP path and timing fabric |
| **Q11** | SMA3 + loopback to GPIO3 | **10 MHz** | Lab/reference output + optional loopback |

---

## How to think about the operating modes

Below are common “mental models” for how you might run the timing fabric, even if the exact selection/priority rules differ depending on how you program inputs and profiles.

### 1) SyncE endpoint (frequency only)
- **CLK0 = SyncE recovered frequency**
- Channel 2 locks to CLK0
- Outputs synthesize stable frequency references (e.g., 25 MHz / 10 MHz), optionally with holdover

### 2) GNSS disciplined (time + frequency)
- **CLK1 = GNSS 1PPS**
- Use PPS to align 1PPS outputs
- Optionally use SyncE on CLK0 as the frequency basis (combo mode), for best stability

### 3) PTP client recovered timing (time from network)
- CM4 recovers time via PTP and provides a PPS into the DPLL (via CLK8/CLK9 depending on routing)
- DPLL aligns outputs to that PPS
- Optionally use SyncE on CLK0 to provide frequency stability (combo)

### 4) Grandmaster mode (PPS out to the system)
- GNSS PPS (or external PPS) disciplines the timing fabric
- The DPLL generates clean, aligned outputs that can be sent to:
  - the CM4 (for GM servo discipline / timestamping workflows)
  - the switch path (for boundary clock / transparent clock experiments)
  - lab SMAs (as references)

---

## Notes on reference selection & priorities

The 8A34004 can be configured to:
- qualify/disqualify references (loss-of-signal, frequency monitoring, activity checks)
- automatically select between references using priority tables
- run revertive or non-revertive behavior
- enter holdover when references disappear

Switchberry’s baseline assumes you will pre-program:
- which inputs exist (frequency, PPS, or both)
- what each input frequency is
- priority order for reference selection

---

## Editing the configuration

### Using Timing Commander
1. Install *Renesas Timing Commander*
2. Open the provided `.tcs`
3. Modify:
   - input muxing
   - reference priorities
   - channel mode (DPLL vs DCO-like usage)
   - loop bandwidth / profiles
   - output frequencies and alignment behavior
4. Export a new programming file for runtime loading

### Programming on hardware
Use the provided programming file with:
- `dplltool` (see `Software/` in this repo), **or**
- your preferred method of writing the device configuration over SPI/I²C as supported on Switchberry

---

## Practical tips

- If you’re combining SyncE + PPS:  
  treat SyncE as the **frequency backbone** and PPS as the **phase/time anchor**.
- For lab work:  
  use Q11 (10 MHz) and a 1PPS output together for easy measurement of phase/frequency stability.
- If you expect reference outages:  
  tune holdover behavior and ensure the TCXO + System DPLL settings match your stability needs.

---

## References

- Renesas 8A34004 product page: https://www.renesas.com/en/products/8a34004
- Renesas 8A34004 datasheet: https://www.renesas.com/document/dst/8a34004-datasheet
- Renesas Timing Commander: https://www.renesas.com/en/software-tool/timing-commander
