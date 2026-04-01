# KSZ9567 Switch CLI

A small Bash CLI for interacting with a **Microchip KSZ9567** Ethernet switch (via Linux SPI / `spidev`) to do common operational tasks like **port status**, **counters**, and **initialization**.

Run everything through the entrypoint:

```bash
./switch_cli.sh <command> [subcommand] [args...]
```

## Features

1. Port status / control (e.g., show link state, enable/disable ports)

2. Counters (read and clear per-port statistics)

3. Initialization (apply a known-good init sequence for your board / wiring)

## Requirements

Linux with SPI enabled and a /dev/spidev* device node

spidev_test available in PATH (or adjust the scripts to point to it)

Typically needs sudo to access /dev/spidev*

```bash
Quick start
chmod +x switch_cli.sh
sudo ./switch_cli.sh --help
```

Usage
Port status
```bash
./switch_cli.sh port status
./switch_cli.sh port status <port> [<port> ...]
```

Enable / disable ports
```bash
sudo ./switch_cli.sh port enable <port> [<port> ...]
sudo ./switch_cli.sh port disable <port> [<port> ...]
```

Read / clear counters
```bash
sudo ./switch_cli.sh counters read [<port> ...]
sudo ./switch_cli.sh counters clear [<port> ...]
```

Initialize the switch
```bash
sudo ./switch_cli.sh switch init
```

Project layout

switch_cli.sh — CLI entrypoint / dispatcher

ksz9567_spi.sh — low-level SPI + register helpers

commands/ — subcommands (ports, counters, switch operations)

Tip: Run switch_cli.sh from the repo root so relative paths resolve correctly.

Safety notes

This tool performs direct register access. Misconfiguration can disrupt traffic.

Run init on a maintenance window the first time, and validate on your target board.

Contributing

PRs welcome—especially for:

Additional subcommands (VLANs, QoS, mirroring, etc.)

Better board/port mapping configuration

Packaging (install script, completion, manpage)