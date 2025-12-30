# Switchberry

Switchberry is a Raspberry Pi CM4–controlled Ethernet switching + timing platform built around a **Microchip KSZ9567** and a **Renesas 8A34004 ClockMatrix DPLL**. It’s designed for **PTP / SyncE / timing lab** workflows while staying flexible enough to run as a compact managed switch/router with precise timing I/O.


![Switchberry board Front view](https://github.com/Time-Appliances-Project/Products/raw/main/Switchberry/Images/Enclosed_FrontView.jpg)
![Switchberry board Rear view](https://github.com/Time-Appliances-Project/Products/raw/main/Switchberry/Images/Enclosed_RearView.jpg)

---

## Order

Coming soon!!!

---

## Key Features

- **5× front-panel RJ45** ports driven by a **KSZ9567** switch
- **Raspberry Pi CM4** controller managing the switch over **SPI**
- **Renesas 8A34004 ClockMatrix DPLL** as the central timing hub
- **Local TCXO** reference for the ClockMatrix (drives derived clocks)
- **OCP M.2 GNSS** slot (GPS/GNSS timing card support)
- **PCIe M.2 2230** slot (Wi-Fi 6E / Wi-Fi 7 modules)
- **4× rear SMA** ports with muxing for multiple input/output routing options
- **SyncE recovery** from the switch into the DPLL (endpoint / boundary clock roles)
- **PTP modes**: hardware transparent clock (typical) or Linux DSA for software-defined behavior
- CM4 ↔ DPLL **PPS routing** supports both **PTP Grandmaster** and **PTP client** workflows

---

## Hardware Overview

### Ethernet Switching (KSZ9567)
- **Switch:** Microchip **KSZ9567**
- **Front panel:** **5× RJ45** ports
- **SFP:** Present on KSZ9567 but **unused** in this design (not used because it doesn’t support PTP)

### Controller / Host (CM4)
- **Compute:** Raspberry Pi **CM4**
- **Switch management:** CM4 controls the KSZ9567 over **SPI**
- **Uplink path to the switch:**
  - CM4 Ethernet MAC → **KSZ9031 PHY** → KSZ9567 (connected as one switch port)

### Timing Core (ClockMatrix)
- **DPLL:** **Renesas 8A34004 ClockMatrix**
  - Central point for all timing signals (distribution, recovery, synthesis)
- **Reference oscillator:** **Local TCXO**
  - Feeds the ClockMatrix and serves as the base reference for derived clocks

### Expansion
- **GNSS:** **OCP M.2 GNSS** slot for GPS/GNSS cards
- **Wi-Fi:** **PCIe M.2 2230** slot for Wi-Fi modules (Wi-Fi 6E / Wi-Fi 7 capable cards)

### Rear I/O (SMA)
- **4× SMA ports** on the back panel
- Each SMA has **muxing**, allowing configurable **input/output** options depending on timing mode and setup

---

## Timing & Sync Capabilities

### SyncE
- Switchberry can **recover the SyncE clock** from the **KSZ9567** and feed it into the **ClockMatrix DPLL**.
- Enables operation as a **SyncE endpoint** or **SyncE boundary clock** (and equivalent roles depending on configuration).

### PTP (IEEE 1588)
The KSZ9567 can be operated in multiple modes depending on the use case:

- **Transparent Clock (typical):**
  - KSZ9567 runs in **transparent clock mode** with **hardware forwarding**.
- **Linux DSA (optional):**
  - Run the switch under the **DSA kernel architecture** to behave more like a **software router**
  - Useful for experimentation with **boundary clock / software-defined forwarding** behavior

### PPS Routing (CM4 ↔ DPLL)
- The **PPS associated with the CM4 Ethernet port** routes to the **DPLL** as **both input and output**.
- Supports:
  - **PTP Grandmaster operation**
    - CM4 disciplines to a **PPS input from the DPLL**, derived from **GNSS** or an **external 1PPS** source.
  - **PTP client operation**
    - CM4 recovers timing via **PTP over the Ethernet switch** and provides **1PPS to the DPLL**.

---

## Typical Use Cases

- **PTP networking**
  - Transparent clock, boundary clock experimentation, lab deployments
- **SyncE networking**
  - Endpoint / boundary clock setups using recovered SyncE into the DPLL
- **PTP Grandmaster**
  - GNSS-referenced or externally referenced timing with PPS discipline
- **PTP client**
  - Recover PPS via PTP and feed the DPLL
- **TSN experimentation**
  - With additional scripting/development
- **DPLL development**
  - Clocking experiments, holdover testing, input qualification, output synthesis
- **Arbitrary frequency generator**
  - Using ClockMatrix outputs via configurable SMA routing
- **PPS ↔ frequency conversion**
  - Convert/derive timing signals via the DPLL and output routing
- **Wi-Fi bridge**
  - Wi-Fi 6E / Wi-Fi 7 connectivity via M.2 2230 (e.g., bridging/edge deployments)

---

## Notes

- The KSZ9567 **SFP port is unused** in this design because it doesn’t support PTP for the intended timing workflows.
- SMA functions are **mode-dependent** and determined by the mux configuration and ClockMatrix setup.

---

