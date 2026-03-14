#!/usr/bin/env python3
"""
find_switchberry.py — Discover Switchberry devices on your local network.

Scans local subnets (or a user-specified range) for hosts responding on
port 8080 with the Switchberry status dashboard.  Works on Windows, macOS,
and Linux.  No dependencies beyond Python 3 stdlib.

Usage:
    python3 find_switchberry.py                         # auto-detect subnets
    python3 find_switchberry.py --subnet 192.168.1.0/24 # scan a specific subnet
    python3 find_switchberry.py --range 10.0.0.1-10.0.0.50  # IP range
    python3 find_switchberry.py --timeout 2 --workers 100
"""
import socket
import subprocess
import sys
import argparse
import urllib.request
import concurrent.futures
import ipaddress
import platform
import re


def get_local_subnets():
    """Discover all local IPv4 subnets from all network interfaces.
    Works on Windows, macOS, and Linux without external dependencies.
    Filters out loopback, link-local, and excessively large subnets."""
    subnets = set()

    if platform.system() == "Windows":
        try:
            output = subprocess.check_output(
                ["powershell", "-Command",
                 "Get-NetIPAddress -AddressFamily IPv4 | "
                 "Where-Object { $_.IPAddress -ne '127.0.0.1' -and $_.PrefixLength -le 30 } | "
                 "ForEach-Object { \"$($_.IPAddress)/$($_.PrefixLength)\" }"],
                text=True, timeout=5
            )
            for line in output.strip().split("\n"):
                line = line.strip()
                if line:
                    try:
                        net = ipaddress.IPv4Network(line, strict=False)
                        subnets.add(str(net))
                    except ValueError:
                        pass
        except Exception:
            pass
    else:
        # Linux / macOS: parse 'ip -4 addr'
        try:
            output = subprocess.check_output(
                ["ip", "-4", "-o", "addr", "show"], text=True, timeout=5
            )
            for match in re.finditer(r'inet (\d+\.\d+\.\d+\.\d+/\d+)', output):
                cidr = match.group(1)
                try:
                    net = ipaddress.IPv4Network(cidr, strict=False)
                    if not net.is_loopback:
                        subnets.add(str(net))
                except ValueError:
                    pass
        except Exception:
            pass

    # Fallback: use the default-route trick
    if not subnets:
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.settimeout(1)
            s.connect(("8.8.8.8", 80))
            local_ip = s.getsockname()[0]
            s.close()
            parts = local_ip.split(".")
            subnets.add(f"{parts[0]}.{parts[1]}.{parts[2]}.0/24")
        except Exception:
            subnets.add("192.168.1.0/24")

    # Filter out link-local (169.254.x.x) and subnets larger than /20 (4094 hosts)
    MAX_PREFIX = 20
    filtered = set()
    for s in subnets:
        net = ipaddress.IPv4Network(s, strict=False)
        if net.is_link_local:
            continue
        if net.prefixlen < MAX_PREFIX:
            continue
        filtered.add(s)

    # If everything was filtered, keep non-link-local ones anyway
    if not filtered:
        filtered = {s for s in subnets
                    if not ipaddress.IPv4Network(s, strict=False).is_link_local}

    return sorted(filtered) if filtered else ["192.168.1.0/24"]


def parse_ip_range(range_str):
    """Parse an IP range like '10.0.0.1-10.0.0.50' into a list of IP strings."""
    parts = range_str.split("-")
    if len(parts) != 2:
        raise ValueError(f"Invalid range format: {range_str} (expected START_IP-END_IP)")
    start = ipaddress.IPv4Address(parts[0].strip())
    end = ipaddress.IPv4Address(parts[1].strip())
    if start > end:
        start, end = end, start
    count = int(end) - int(start) + 1
    if count > 65536:
        raise ValueError(f"Range too large: {count} hosts (max 65536)")
    return [str(ipaddress.IPv4Address(int(start) + i)) for i in range(count)]


def check_host(ip_str, port=8080, timeout=1.5):
    """Check if a host is a Switchberry by trying to fetch its status page."""
    try:
        # Quick TCP connect check first
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        result = sock.connect_ex((ip_str, port))
        sock.close()
        if result != 0:
            return None

        # Port is open — try to fetch the page
        url = f"http://{ip_str}:{port}/"
        req = urllib.request.Request(url, headers={"User-Agent": "SwitchberryFinder/1.0"})
        resp = urllib.request.urlopen(req, timeout=timeout)
        body = resp.read(4096).decode("utf-8", errors="replace")

        if "Switchberry" in body:
            # Extract hostname from title
            hostname = "Unknown"
            if "<title>" in body:
                start = body.index("<title>") + len("<title>")
                end = body.index("</title>", start)
                title = body[start:end]
                # Title format: "Switchberry Status — hostname"
                if "\u2014" in title:
                    hostname = title.split("\u2014")[1].strip()
                elif "-" in title:
                    hostname = title.split("-")[1].strip()

            return {
                "ip": ip_str,
                "port": port,
                "hostname": hostname,
                "url": f"http://{ip_str}:{port}",
            }
    except Exception:
        pass
    return None


def main():
    parser = argparse.ArgumentParser(
        description="🍓 Find Switchberry timing appliances on your local network.\n\n"
                    "Scans for devices running the Switchberry status dashboard on port 8080.\n"
                    "Auto-detects local subnets by default, or specify a target manually.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="Examples:\n"
               "  python3 find_switchberry.py                              # auto-detect\n"
               "  python3 find_switchberry.py --subnet 192.168.1.0/24      # specific subnet\n"
               "  python3 find_switchberry.py --subnet 10.0.0.0/22         # larger subnet\n"
               "  python3 find_switchberry.py --range 192.168.1.100-192.168.1.200  # IP range\n"
               "  python3 find_switchberry.py --timeout 3 --workers 100    # tune performance\n"
    )
    parser.add_argument("--subnet", "-s", default=None,
                        help="Subnet to scan in CIDR notation (e.g. 192.168.1.0/24)")
    parser.add_argument("--range", "-r", default=None, dest="ip_range",
                        help="IP range to scan (e.g. 192.168.1.100-192.168.1.200)")
    parser.add_argument("--port", "-p", type=int, default=8080,
                        help="Port to check (default: 8080)")
    parser.add_argument("--timeout", "-t", type=float, default=1.5,
                        help="Timeout per host in seconds (default: 1.5)")
    parser.add_argument("--workers", "-w", type=int, default=50,
                        help="Number of concurrent scan workers (default: 50)")
    args = parser.parse_args()

    print("🍓 Switchberry Network Discovery")

    # Determine which hosts to scan
    hosts = []
    if args.ip_range:
        try:
            hosts = parse_ip_range(args.ip_range)
            print(f"   Scanning range {args.ip_range} ({len(hosts)} hosts) on port {args.port}...")
        except ValueError as e:
            print(f"Error: {e}")
            sys.exit(1)
    elif args.subnet:
        try:
            network = ipaddress.IPv4Network(args.subnet, strict=False)
            hosts = [str(ip) for ip in network.hosts()]
            print(f"   Scanning {args.subnet} ({len(hosts)} hosts) on port {args.port}...")
        except ValueError as e:
            print(f"Error: Invalid subnet: {e}")
            sys.exit(1)
    else:
        subnets = get_local_subnets()
        all_hosts = set()
        for subnet in subnets:
            print(f"   Scanning {subnet} on port {args.port}...")
            try:
                network = ipaddress.IPv4Network(subnet, strict=False)
                for ip in network.hosts():
                    all_hosts.add(str(ip))
            except ValueError:
                pass
        hosts = sorted(all_hosts, key=lambda x: ipaddress.IPv4Address(x))

    print()
    if not hosts:
        print("No hosts to scan.")
        sys.exit(1)

    found = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as pool:
        futures = {pool.submit(check_host, ip, args.port, args.timeout): ip for ip in hosts}
        done_count = 0
        total = len(hosts)
        for future in concurrent.futures.as_completed(futures):
            done_count += 1
            if done_count % 25 == 0 or done_count == total:
                pct = int(done_count / total * 100)
                print(f"\r   Scanned {done_count}/{total} ({pct}%)", end="", flush=True)
            result = future.result()
            if result:
                found.append(result)

    print("\r" + " " * 40 + "\r", end="")  # Clear progress line

    if found:
        # Group by hostname to show all IPs per device
        devices = {}
        for dev in found:
            hostname = dev["hostname"]
            if hostname not in devices:
                devices[hostname] = []
            devices[hostname].append(dev)

        print(f"Found {len(found)} Switchberry endpoint(s) on {len(devices)} device(s):\n")
        for hostname, entries in sorted(devices.items()):
            print(f"  🟢 {hostname}")
            for entry in sorted(entries, key=lambda d: ipaddress.IPv4Address(d["ip"])):
                print(f"     {entry['ip']}")
                print(f"       HTTP:  http://{entry['ip']}:{args.port}")
                print(f"       HTTPS: https://{entry['ip']}:{args.port + 363}")
            print()
    else:
        print("No Switchberry devices found.")
        if not args.subnet and not args.ip_range:
            print("  Try specifying a subnet: --subnet 192.168.1.0/24")
            print("  Or an IP range:          --range 192.168.1.1-192.168.1.254")
        print("  Check that the device is powered on and port 8080 is accessible.")


if __name__ == "__main__":
    main()
