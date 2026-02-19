#!/usr/bin/env python3
"""
find_switchberry.py — Discover Switchberry devices on your local network.

Scans the local subnet for hosts responding on port 8080 with
the Switchberry status dashboard. Works on Windows, macOS, and Linux.
No dependencies beyond Python 3 stdlib.

Usage:
    python3 find_switchberry.py
    python3 find_switchberry.py --subnet 192.168.1.0/24
    python3 find_switchberry.py --timeout 2
"""
import socket
import subprocess
import sys
import argparse
import urllib.request
import concurrent.futures
import ipaddress
import platform


def get_local_subnet():
    """Try to determine the local subnet from the default interface."""
    try:
        # Connect to a public IP to find our local address (doesn't send data)
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.settimeout(1)
        s.connect(("8.8.8.8", 80))
        local_ip = s.getsockname()[0]
        s.close()
        # Assume /24 subnet
        parts = local_ip.split(".")
        return f"{parts[0]}.{parts[1]}.{parts[2]}.0/24"
    except Exception:
        return "192.168.1.0/24"


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
                if "—" in title:
                    hostname = title.split("—")[1].strip()
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
        description="Find Switchberry devices on your local network",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="Examples:\n"
               "  python3 find_switchberry.py\n"
               "  python3 find_switchberry.py --subnet 10.1.1.0/24\n"
               "  python3 find_switchberry.py --timeout 3\n"
    )
    parser.add_argument("--subnet", "-s", default=None,
                        help="Subnet to scan (default: auto-detect, e.g. 192.168.1.0/24)")
    parser.add_argument("--port", "-p", type=int, default=8080,
                        help="Port to check (default: 8080)")
    parser.add_argument("--timeout", "-t", type=float, default=1.5,
                        help="Timeout per host in seconds (default: 1.5)")
    parser.add_argument("--workers", "-w", type=int, default=50,
                        help="Number of concurrent workers (default: 50)")
    args = parser.parse_args()

    subnet = args.subnet or get_local_subnet()
    print(f"🍓 Switchberry Network Discovery")
    print(f"   Scanning {subnet} on port {args.port}...")
    print()

    try:
        network = ipaddress.IPv4Network(subnet, strict=False)
    except ValueError as e:
        print(f"Error: Invalid subnet: {e}")
        sys.exit(1)

    hosts = [str(ip) for ip in network.hosts()]
    found = []

    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as pool:
        futures = {pool.submit(check_host, ip, args.port, args.timeout): ip for ip in hosts}
        done_count = 0
        total = len(hosts)
        for future in concurrent.futures.as_completed(futures):
            done_count += 1
            # Progress indicator
            if done_count % 25 == 0 or done_count == total:
                pct = int(done_count / total * 100)
                print(f"\r   Scanned {done_count}/{total} ({pct}%)", end="", flush=True)
            result = future.result()
            if result:
                found.append(result)

    print("\r" + " " * 40 + "\r", end="")  # Clear progress line

    if found:
        print(f"Found {len(found)} Switchberry device(s):\n")
        for dev in sorted(found, key=lambda d: d["ip"]):
            print(f"  🟢 {dev['hostname']}")
            print(f"     IP:    {dev['ip']}")
            print(f"     HTTP:  http://{dev['ip']}:{args.port}")
            print(f"     HTTPS: https://{dev['ip']}:{args.port + 363}")
            print()
    else:
        print("No Switchberry devices found on this subnet.")
        print(f"  Scanned: {subnet} port {args.port}")
        print("  Try: --subnet <your-subnet> or check if the device is powered on.")


if __name__ == "__main__":
    main()
