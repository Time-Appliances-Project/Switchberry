# Switchberry Timing Quick Start

This guide gets you from “fresh install” to a working timing/PTP setup, and shows how to customize behavior using the `config.py` wizard and the daemon Makefile.

**↑ [Back to main README](../README.md)**

## Table of Contents

- [System Overview](#system-overview)
- [Default behavior (out of the box)](#default-behavior-out-of-the-box)
- [Quick start: customize your timing/PTP setup](#quick-start-customize-your-timingptp-setup)
- [Detailed setup](#detailed-setup)

**Related Guides:** [Switch CLI](switch/README.md) · [Daemons & Services](daemons/README.md) · [ClockMatrix Utilities](clockmatrix/README.md)

### System Overview

```mermaid
graph TD
    subgraph Sources [Timing Sources]
        GPS[GPS Receiver]
        SyncE[SyncE Network]
        SMA_In[SMA Inputs]
    end

    subgraph Switchberry
        direction TB
        DPLL[8A34004 DPLL]
        CM4[Compute Module 4]
        Switch[KSZ9567 Switch]
    end

    subgraph Outputs [Timing Distribution]
        PTP[PTP Network]
        SMA_Out[SMA Outputs]
    end

    %% Flow
    GPS -->|1PPS| DPLL
    SMA_In -->|Freq/Time| DPLL
    SyncE -->|Recovered Clock| Switch
    Switch -->|25MHz Ref| DPLL

    DPLL -->|1PPS| CM4
    DPLL -->|Freq & 1PPS| SMA_Out
    
    CM4 -->|System Time| PTP
    Switch -->|PTP Packets| PTP
```

---

## Default behavior (out of the box)

If you haven’t customized `/etc/startup-dpll.json`, the default setup is:

### PTP
- **Role:** **Grandmaster (GM)**
- **Mode:** **Unicast** (reduces message flooding; safer on shared networks)

### Input priorities
- **Time sources**
  1. **GPS** (highest priority for time)
  2. **SMA4** = 1PPS input (fallback time behind GPS)

- **Frequency sources**
  1. **SMA1** = 10MHz input (highest priority for frequency)

### Outputs
- **SMA3** = 1PPS output (or other frequency)
- **SMA2** = 10MHz output (**frequency-only**, V6 board - not phase-aligned)

> **V6 Board Note:** **SMA2** is routed via Channel 5 (SyncE frequency channel) and only
> provides frequency alignment, not phase alignment. For phase-aligned 1PPS output,
> use **SMA4** instead (which is routed via Channel 6).

### Advanced: Hardware SMA Mapping
For users debugging schematics or low-level GPIO code, here is the mapping between Rear-Panel labels (User) and Schematic labels (Hardware):

| User Label (Rear) | Hardware Label (Schematic) |
| :--- | :--- |
| **SMA1** | SMA4 |
| **SMA2** | SMA3 |
| **SMA3** | SMA2 |
| **SMA4** | SMA1 |

### NTP Server (GM+GPS only)

When configured as a **PTP Grandmaster with GPS**, Switchberry automatically serves **NTP** using [chrony](https://chrony-project.org/) with the CM4 Ethernet NIC PHC (`/dev/ptp0`) as a Stratum 1 refclock. The PHC is disciplined by `ts2phc` from GPS.

- **Guard script** (`switchberry_chrony_guard.sh`): Only starts `chronyd` once `ts2phc` has converged. If GPS is lost or the DPLL goes unstable, chrony is stopped so stale time is never served. It restarts automatically when GPS recovers.
- **Config:** `/etc/switchberry/chrony-switchberry.conf`
- **Dependency:** `chrony` must be installed (`sudo apt install chrony`). The stock `chronyd.service` is automatically stopped to avoid port conflicts.
- NTP is **not served** in CLIENT or NONE modes.

### System Clock Sync (GM+GPS and CLIENT)

The system clock (`CLOCK_REALTIME`) is automatically synced from the PHC via `phc2sys` in both **GM+GPS** and **CLIENT** modes:

- **GM+GPS:** `phc2sys` starts when `ts2phc` converges (PHC has GPS time).
- **CLIENT:** `phc2sys` starts when `ptp4l` is locked (servo `s2`, tight offsets).
- **Guard script** (`switchberry_phc2sys_guard.sh`): Stops `phc2sys` when the upstream time source is lost, restarts when recovered.

---

## Quick start: customize your timing/PTP setup

### 1) Clone the github software
```bash
cd ~/
git clone https://github.com/Time-Appliances-Project/Switchberry
cd Switchberry/Software 
git config core.fileMode false
chmod +x sb-*.sh install_all.sh
```

### 2) Run the config wizard
This script runs the wizard and automatically installs the generated configuration to `/etc/startup-dpll.json`.

```bash
./sb-config.sh
```

### 3) Apply changes and restart services
This script recompiles tools, installs services, and restarts the PTP stack.

```bash
sudo ./sb-reinstall.sh
```

### 4) Check Status
Monitor DPLL lock state, PTP synchronization, and service health.

```bash
./sb-status.sh
```

### 5) Web Status Dashboard
A built-in web server runs on port **8080** (HTTP) and **8443** (HTTPS), showing a live dashboard with system identity, configuration, service status, DPLL state, and PTP sync info. It auto-refreshes every 10 seconds.

From any browser on the same network, open:
```
http://<switchberry-ip>:8080
```

> **Tip:** Use the `find_switchberry.py` script (in the repo root) from your PC to automatically discover Switchberry devices on your LAN:
> ```bash
> python3 find_switchberry.py
> ```

### 6) (Optional) Customize PTP configuration
If you need to change PTP profiles (e.g. Unicast vs Multicast), edit the config files managed by the services:
- Client: `~/Switchberry/Software/daemons/ptp4l-switchberry-client.conf`
- Grandmaster: `~/Switchberry/Software/daemons/ptp4l-switchberry-gm.conf`
Then run `sudo ./sb-reinstall.sh` to apply.






## Detailed setup 

This section is only if you want to start from a fresh SD card for instance. 

To access the Switchberry, you can use 

1. Micro-HDMI for monitor, USB-A for keyboard
2. Micro-USB on side. This will come up as UART to the CM4.
	To access this with a fresh SD card, you'll need to edit config.txt into the SD card to enable the UART.

In config.txt, add these at the end under [all]:

```bash
dtparam=spi=on
enable_uart=1
uart_2ndstage=1
dtoverlay=disable-bt
dtoverlay=switchberrytc
#dtoverlay=ksz9567
dtoverlay=spi0-1cs
dtoverlay=uart5,txd5_pin=12,rxd5_pin=13
dtoverlay=pcie-32bit-dma
dtparam=i2c_vc=off
hdmi_force_hotplug=1
```

### 1) Setup SD-card
Edit config.txt in bootfs of SD card like above. Enables UART and boot without monitor.

### 2) Clone the github software
```bash
cd ~/
git clone https://github.com/Time-Appliances-Project/Switchberry
cd Switchberry 
```

### 3) Install a bunch of dependencies

```bash
sudo apt update
sudo apt install bc bison flex libssl-dev make screen vim automake iperf tshark autoconf libmnl-dev libmnl-doc jq gpsd gpsd-clients socat linuxptp 
```

### 4) Clone kernel sources
Instructions for building CM4 kernel are here: https://www.raspberrypi.com/documentation/computers/linux_kernel.html

```bash 
cd ~/
mkdir kernel
cd kernel
git clone --depth=1 https://github.com/raspberrypi/linux
cd linux
# updated .config with additional items needed by switchberry like MDIO and others 
cp ~/Switchberry/Software/kernel/.config ./.config
# device tree overlays for switchberry, for both default ("Transparent clock") and DSA ("Boundary clock"), and Makefile
cp ~/Switchberry/Software/kernel/ksz9567-overlay.dts arch/arm/boot/dts/overlays/
cp ~/Switchberry/Software/kernel/switchberrytc-overlay.dts arch/arm/boot/dts/overlays/
cp ~/Switchberry/Software/kernel/copy_Makefile arch/arm/boot/dts/overlays/Makefile
```

### 5) Rebuild kernel with updated config and overlays

This step will take significant time!
```bash 
cd ~/kernel/linux
KERNEL=kernel8

# THIS STEP ESPECIALLY takes a long time , I run it in screen so if you disconnect it keeps going
#screen -S test # do this if you're familiar with screen so ssh connection can drop 
# if it asks questions, just hold enter for defaults
make -j6 Image.gz modules dtbs ; sudo make -j6 modules_install

KERNEL=kernel8
sudo cp /boot/firmware/$KERNEL.img /boot/firmware/$KERNEL-backup.img
sudo cp arch/arm64/boot/Image.gz /boot/firmware/$KERNEL.img
sudo cp arch/arm64/boot/dts/broadcom/*.dtb /boot/firmware/
sudo cp arch/arm64/boot/dts/overlays/*.dtb* /boot/firmware/overlays/
sudo cp arch/arm64/boot/dts/overlays/README /boot/firmware/overlays/
```

### 6) Reboot

```bash
sudo reboot
```

### 7) Install third party dependencies

#### spidev-test

```bash
cd ~/kernel/linux/tools/spi
make clean; make all; sudo make install
```

#### mdio-tools

```bash
cd ~/
git clone https://github.com/wkz/mdio-tools
cd mdio-tools/kernel
KDIR=~/kernel/linux/
make all; sudo make install 
cd .. 
./autogen.sh
./configure --prefix=/usr && make all && sudo make install
```

#### testptp

```bash
cd ~/kernel/linux/tools/testing/selftests/ptp
gcc -Wall -lrt testptp.c -o testptp
sudo cp testptp /usr/bin/
```


### 8) Build & Install everything for Switchberry

```bash
cd ~/Switchberry/Software/
chmod +x install_all.sh
sudo ./install_all.sh
```

### 9) Customize 

Follow quick-start guide above at this point, everything should be installed!

You need to create a /etc/startup-dpll.json for everything to get configured and operate properly.






