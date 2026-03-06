#!/usr/bin/env python3
"""
generate_ptp_conf.py — Generate ptp4l configuration files from startup-dpll.json.

Reads the PTP role and transport settings from the JSON config and writes
the appropriate ptp4l .conf file(s) into /etc/switchberry/.

Usage:
    python3 generate_ptp_conf.py [--config /etc/startup-dpll.json] [--outdir /etc/switchberry]
"""
import json
import argparse
import os
import sys

DEFAULT_CONFIG = "/etc/startup-dpll.json"
DEFAULT_OUTDIR = "/etc/switchberry"


def generate_gm_unicast_conf(time_traceable: bool = True) -> str:
    clock_class = 6 if time_traceable else 248
    clock_accuracy = "0x21" if time_traceable else "0xFE"  # 100ns (GPS) : unknown
    return f"""\
[global]
serverOnly\t\t\t1
hybrid_e2e\t\t\t0
inhibit_multicast_service\t1
unicast_listen\t\t\t1
unicast_req_duration\t\t60
announceReceiptTimeout\t\t2
tx_timestamp_timeout\t\t10000
ptp_minor_version\t\t0
utc_offset\t\t\t37
leapfile\t\t\t/usr/share/zoneinfo/leap-seconds.list
clockClass\t\t\t{clock_class}
clockAccuracy\t\t\t{clock_accuracy}
[eth0]
"""


def generate_gm_multicast_conf(time_traceable: bool = True) -> str:
    clock_class = 6 if time_traceable else 248
    clock_accuracy = "0x21" if time_traceable else "0xFE"
    return f"""\
[global]
serverOnly\t\t\t1
hybrid_e2e\t\t\t0
inhibit_multicast_service\t0
announceReceiptTimeout\t\t3
tx_timestamp_timeout\t\t10000
ptp_minor_version\t\t0
utc_offset\t\t\t37
leapfile\t\t\t/usr/share/zoneinfo/leap-seconds.list
clockClass\t\t\t{clock_class}
clockAccuracy\t\t\t{clock_accuracy}
[eth0]
"""


def generate_client_unicast_conf(master_ip: str) -> str:
    return f"""\
[global]
dataset_comparison\t\tG.8275.x
G.8275.defaultDS.localPriority\t128
serverOnly\t\t\t0
clientOnly\t\t\t1
G.8275.portDS.localPriority\t128
hybrid_e2e\t\t\t0
inhibit_multicast_service\t1
unicast_listen\t\t\t1
unicast_req_duration\t\t60
announceReceiptTimeout\t\t20
tx_timestamp_timeout\t\t10000
free_running\t\t\t0
first_step_threshold\t\t1
logSyncInterval\t\t\t0
ptp_minor_version\t\t0
utc_offset\t\t\t37
leapfile\t\t\t/usr/share/zoneinfo/leap-seconds.list
#
[unicast_master_table]
table_id\t\t\t1
logQueryInterval\t\t0
UDPv4\t\t\t\t{master_ip}
#
[eth0]
unicast_master_table\t\t1
"""


def generate_client_multicast_conf() -> str:
    return """\
[global]
dataset_comparison\t\tG.8275.x
G.8275.defaultDS.localPriority\t128
serverOnly\t\t\t0
clientOnly\t\t\t1
G.8275.portDS.localPriority\t128
hybrid_e2e\t\t\t0
inhibit_multicast_service\t0
announceReceiptTimeout\t\t3
tx_timestamp_timeout\t\t10000
free_running\t\t\t0
first_step_threshold\t\t1
logSyncInterval\t\t\t0
ptp_minor_version\t\t0
utc_offset\t\t\t37
leapfile\t\t\t/usr/share/zoneinfo/leap-seconds.list
[eth0]
"""


def main():
    parser = argparse.ArgumentParser(description="Generate ptp4l conf files from Switchberry config")
    parser.add_argument("--config", "-c", default=DEFAULT_CONFIG, help="Path to startup-dpll.json")
    parser.add_argument("--outdir", "-o", default=DEFAULT_OUTDIR, help="Output directory for conf files")
    args = parser.parse_args()

    if not os.path.isfile(args.config):
        print(f"Error: config file not found: {args.config}", file=sys.stderr)
        sys.exit(1)

    with open(args.config) as f:
        data = json.load(f)

    role = data.get("ptp_role", "NONE")
    ptp = data.get("ptp", {})
    transport = ptp.get("transport", "UNICAST").upper()
    master_ip = ptp.get("master_ip", "10.1.1.11")
    time_traceable = ptp.get("time_traceable", True)

    os.makedirs(args.outdir, exist_ok=True)

    if role == "GM":
        if transport == "MULTICAST":
            conf = generate_gm_multicast_conf(time_traceable)
        else:
            conf = generate_gm_unicast_conf(time_traceable)

        out_path = os.path.join(args.outdir, "ptp4l-switchberry-gm.conf")
        with open(out_path, "w") as f:
            f.write(conf)
        print(f"Generated GM ({transport}) config: {out_path}")

    elif role == "CLIENT":
        if transport == "MULTICAST":
            conf = generate_client_multicast_conf()
        else:
            conf = generate_client_unicast_conf(master_ip)

        out_path = os.path.join(args.outdir, "ptp4l-switchberry-client.conf")
        with open(out_path, "w") as f:
            f.write(conf)
        print(f"Generated CLIENT ({transport}) config: {out_path}")

    else:
        print(f"PTP role is '{role}', no ptp4l config generated.")


if __name__ == "__main__":
    main()
