# Switchberry Timing Quick Start

This guide gets you from “fresh install” to a working timing/PTP setup, and shows how to customize behavior using the `config.py` wizard and the daemon Makefile.

---

## Default behavior (out of the box)

If you haven’t customized `/etc/startup-dpll.json`, the default setup is:

### PTP
- **Role:** **Grandmaster (GM)**
- **Mode:** **Unicast** (reduces message flooding; safer on shared networks)

### Input priorities
- **Time sources**
  1. **GPS** (highest priority for time)
  2. **SMA1 = 1PPS input** (fallback time behind GPS)

- **Frequency sources**
  1. **SMA4 = 10MHz input** (highest priority for frequency)

### Outputs
- **SMA2 = 1PPS output**
- **SMA3 = 10MHz output**

---

## Quick start: customize your timing/PTP setup

### 1) Clone the github software
```bash
cd ~/
git clone https://github.com/Time-Appliances-Project/Switchberry
cd Switchberry 
```

### 2) Run the config wizard

From the Switchberry repo directory:

```bash
cd ~/Switchberry/Software/clockmatrix
python3 config.py wizard
```

After answering wizard questions, you can view a summary of the config and validate it:

```bash
cd ~/Switchberry/Software/clockmatrix
python3 config.py validate -c dpll-config.json
```

### 3) Copy this setup file to default location

This will define how switchberry will operate, including SMA operations and PTP operations

```bash
cd ~/Switchberry/Software/clockmatrix
cp dpll-config.json /etc/startup-dpll.json
```

### 4) Setup your ptp4l configuration (if using PTP)

If using PTP4L as a client, change this file 

```bash
~/Switchberry/Software/daemons/ptp4l-switchberry-client-uc.conf
```

If using PTP4l as a Grandmaster, change this file

```bash
~/Switchberry/Software/daemons/ptp4l-switchberry-gm-uc.conf
```

### 5) Restart the Switchberry systemd processes

Switchberry operation is managed and runs through systemd processes, this will restart them with this new config.

```bash
cd ~/Switchberry/Software/daemons
sudo make install 
sudo make restart
```






## Detailed setup 

This section is only if you want to start from a fresh SD card for instance. 

To access the Switchberry, you can use 

1. Micro-HDMI for monitor, USB-A for keyboard
2. Micro-USB on side. This will come up as UART to the CM4.
	To access this with a fresh SD card, you'll need to copy cmdline.txt and config.txt into the SD card to enable the UART.

### 1) Clone the github software
```bash
cd ~/
git clone https://github.com/Time-Appliances-Project/Switchberry
cd Switchberry 
```

### 2) Clone kernel sources
Instructions for building CM4 kernel are here: https://www.raspberrypi.com/documentation/computers/linux_kernel.html

```bash 
cd ~/
mkdir kernel
cd kernel
git clone --depth=1 https://github.com/raspberrypi/linux
cd linux
# updated .config with additional items needed by switchberry like MDIO and others 
cp ~/Switchberry/kernel/.config ./.config
# device tree overlays for switchberry, for both default ("Transparent clock") and DSA ("Boundary clock")
cp ~/Switchberry/kernel/ksz9567-overlay.dts arch/arm/boot/dts/overlays/
cp ~/Switchberry/kernel/switchberrytc-overlay.dts arch/arm/boot/dts/overlays/
# config.txt and cmdline.txt to enable uart and other peripherals used by switchberry 
sudo cp ~/Switchberry/kernel/config.txt /boot/firmware/
sudo cp ~/Switchberry/kernel/cmdline.txt /boot/firmware/
```

### 3) Rebuild kernel with updated config and overlays

This step will take significant time!
```bash 
cd ~/kernel/linux
KERNEL=kernel8

# THIS STEP ESPECIALLY takes a long time 
make -j6 Image.gz modules dtbs ; sudo make -j6 modules_install

sudo cp /boot/firmware/$KERNEL.img /boot/firmware/$KERNEL-backup.img
sudo cp arch/arm64/boot/Image.gz /boot/firmware/$KERNEL.img
sudo cp arch/arm64/boot/dts/broadcom/*.dtb /boot/firmware/
sudo cp arch/arm64/boot/dts/overlays/*.dtb* /boot/firmware/overlays/
sudo cp arch/arm64/boot/dts/overlays/README /boot/firmware/overlays/
```

### 4) Reboot

```bash
reboot
```


### 5) Install third party dependencies

#### spidev-test

```bash
cd ~/kernel/linux/tools/spi
make clean; make all; sudo make install
```

#### MDIO-tools

```bash
cd ~/
git clone https://github.com/wkz/mdio-tools
cd mdio-tools/kernel
KDIR=~/kernel/linux/
make all; sudo make install 
```

### 6) Install everything for Switchberry

```bash
cd ~/Switchberry/Software/
chmod +x install_all.sh
sudo ./install_all.sh
```

### 7) Customize 

Follow quick-start guide above at this point, everything should be installed!

You need to create a /etc/startup-dpll.json for everything to get configured and operate properly.








