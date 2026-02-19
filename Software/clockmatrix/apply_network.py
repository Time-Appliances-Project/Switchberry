#!/usr/bin/env python3
"""
apply_network.py — Apply eth0 network configuration from startup-dpll.json.

Reads the 'network' section and configures eth0 via NetworkManager (nmcli).

Usage:
    sudo python3 apply_network.py [--config /etc/startup-dpll.json] [--dry-run]
"""
import json
import argparse
import os
import sys
import subprocess

DEFAULT_CONFIG = "/etc/startup-dpll.json"
# NetworkManager connection name for eth0 (use 'nmcli con show' to verify)
NM_CON_NAME = "Wired connection 1"


def nmcli(*args, dry_run=False):
    """Run an nmcli command. In dry-run mode, just print it."""
    cmd = ["nmcli"] + list(args)
    if dry_run:
        print(f"  [DRY RUN] {' '.join(cmd)}")
        return
    print(f"  $ {' '.join(cmd)}")
    subprocess.run(cmd, check=False)


def find_eth0_connection():
    """Try to find the NM connection name for eth0."""
    try:
        out = subprocess.run(
            ["nmcli", "-t", "-f", "NAME,DEVICE", "con", "show"],
            capture_output=True, text=True, check=False
        )
        for line in out.stdout.strip().split("\n"):
            parts = line.split(":")
            if len(parts) >= 2 and parts[1] == "eth0":
                return parts[0]
    except Exception:
        pass
    return NM_CON_NAME  # fallback


def main():
    parser = argparse.ArgumentParser(description="Apply eth0 network config from Switchberry JSON")
    parser.add_argument("--config", "-c", default=DEFAULT_CONFIG, help="Path to startup-dpll.json")
    parser.add_argument("--dry-run", action="store_true", help="Print changes without applying")
    args = parser.parse_args()

    if not os.path.isfile(args.config):
        print(f"Error: config file not found: {args.config}", file=sys.stderr)
        sys.exit(1)

    with open(args.config) as f:
        data = json.load(f)

    net = data.get("network", {})
    mode = net.get("mode", "DHCP").upper()

    con_name = find_eth0_connection()
    print(f"Using NetworkManager connection: '{con_name}'")

    if mode == "STATIC":
        ip = net.get("ip_address")
        cidr = net.get("cidr", 24)
        gw = net.get("gateway")
        dns = net.get("dns")
        if not ip:
            print("Error: STATIC mode requires ip_address in config", file=sys.stderr)
            sys.exit(1)

        print(f"Configuring eth0: STATIC {ip}/{cidr} gw={gw} dns={dns}")
        nmcli("con", "mod", con_name, "ipv4.method", "manual",
              "ipv4.addresses", f"{ip}/{cidr}", dry_run=args.dry_run)
        if gw:
            nmcli("con", "mod", con_name, "ipv4.gateway", gw, dry_run=args.dry_run)
        else:
            nmcli("con", "mod", con_name, "ipv4.gateway", "", dry_run=args.dry_run)
        if dns:
            nmcli("con", "mod", con_name, "ipv4.dns", dns, dry_run=args.dry_run)
        else:
            nmcli("con", "mod", con_name, "ipv4.dns", "", dry_run=args.dry_run)
    else:
        print("Configuring eth0: DHCP (auto)")
        # Clear each property individually — single combined call is unreliable
        nmcli("con", "mod", con_name, "ipv4.method", "auto", dry_run=args.dry_run)
        nmcli("con", "mod", con_name, "ipv4.addresses", "", dry_run=args.dry_run)
        nmcli("con", "mod", con_name, "ipv4.gateway", "", dry_run=args.dry_run)
        nmcli("con", "mod", con_name, "ipv4.dns", "", dry_run=args.dry_run)

    # If wlan0 exists, set eth0 route metric high so wifi stays as primary route
    if os.path.exists("/sys/class/net/wlan0"):
        print("wlan0 detected — setting eth0 route-metric to 9999")
        nmcli("con", "mod", con_name, "ipv4.route-metric", "9999", dry_run=args.dry_run)

    # Fire-and-forget: try to bring the connection up without waiting.
    # If eth0 has no carrier yet, nmcli will time out on its own — we don't block.
    if not args.dry_run:
        print("Requesting connection activation (not waiting)...")
        subprocess.Popen(["nmcli", "con", "up", con_name],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print("Done.")


if __name__ == "__main__":
    main()
