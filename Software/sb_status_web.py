#!/usr/bin/env python3
"""
sb_status_web.py — Status web server for Switchberry.

Serves an auto-refreshing HTML dashboard on port 8080 (HTTP) and 8443 (HTTPS).
Shows system identity, configuration, service status with log tails,
DPLL hardware, PTP sync status, and SMA configuration.

Uses only Python stdlib. HTTPS uses a self-signed certificate
auto-generated on first run.

Usage:
    sudo python3 sb_status_web.py [--port 8080] [--https-port 8443]
"""
import http.server
import ssl
import subprocess
import socket
import json
import os
import argparse
import html
import threading
import tempfile

DEFAULT_PORT = 8080
DEFAULT_HTTPS_PORT = 8443
CONFIG_PATH = "/etc/startup-dpll.json"
DPLLTOOL = "/usr/local/sbin/dplltool"
CERT_DIR = "/etc/switchberry"
CERT_FILE = os.path.join(CERT_DIR, "status-web.pem")

SERVICES = [
    "switchberry-sanity.service",
    "switchberry-apply-network.service",
    "ptp4l-switchberry-client.service",
    "ptp4l-switchberry-gm.service",
    "switchberry-cm4-pps-monitor.service",
    "switchberry-dpll-monitor.service",
    "switchberry-ptp-role.service",
    "ts2phc-switchberry.service",
    "switchberry-status-web.service",
]

LOG_LINES = 3  # Number of journal lines per service


def run_cmd(cmd, timeout=5):
    """Run a command and return stdout, or error string."""
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        return r.stdout.strip()
    except Exception as e:
        return f"Error: {e}"


def esc(val):
    """html.escape that handles None gracefully."""
    if val is None:
        return "N/A"
    return html.escape(str(val))


def ensure_self_signed_cert():
    """Generate a self-signed cert if one doesn't exist."""
    if os.path.isfile(CERT_FILE):
        return CERT_FILE
    try:
        os.makedirs(CERT_DIR, exist_ok=True)
        hostname = socket.gethostname()
        subprocess.run([
            "openssl", "req", "-x509", "-newkey", "rsa:2048",
            "-keyout", CERT_FILE, "-out", CERT_FILE,
            "-days", "3650", "-nodes",
            "-subj", f"/CN={hostname}"
        ], check=True, capture_output=True)
        os.chmod(CERT_FILE, 0o600)
        print(f"Generated self-signed certificate: {CERT_FILE}")
    except Exception as e:
        print(f"Warning: Could not generate HTTPS cert: {e}")
        return None
    return CERT_FILE


def get_hostname():
    return socket.gethostname()


def get_eth0_info():
    """Get eth0 MAC and IP address."""
    mac = "Unknown"
    ip = "Unknown"
    try:
        out = run_cmd(["ip", "-br", "addr", "show", "eth0"])
        parts = out.split()
        if len(parts) >= 3:
            ip = parts[2]
    except Exception:
        pass
    try:
        mac_path = "/sys/class/net/eth0/address"
        if os.path.isfile(mac_path):
            with open(mac_path) as f:
                mac = f.read().strip()
    except Exception:
        pass
    return mac, ip


def get_config_summary():
    """Parse and summarize the JSON config."""
    if not os.path.isfile(CONFIG_PATH):
        return {"error": f"{CONFIG_PATH} not found"}
    try:
        with open(CONFIG_PATH) as f:
            d = json.load(f)
        return {
            "ptp_role": d.get("ptp_role", "Unknown"),
            "ptp_transport": d.get("ptp", {}).get("transport", "UNICAST"),
            "ptp_master_ip": d.get("ptp", {}).get("master_ip") or "N/A",
            "gps_present": d.get("gps", {}).get("present", False),
            "gps_role": d.get("gps", {}).get("role") or "N/A",
            "cm4_used": d.get("cm4", {}).get("used_as_source", False),
            "synce_used": d.get("synce", {}).get("used_as_source", False),
            "network_mode": d.get("network", {}).get("mode", "DHCP"),
            "network_ip": d.get("network", {}).get("ip_address") or "N/A",
        }
    except Exception as e:
        return {"error": str(e)}


def get_sma_config():
    """Parse SMA configuration from JSON config."""
    if not os.path.isfile(CONFIG_PATH):
        return []
    try:
        with open(CONFIG_PATH) as f:
            d = json.load(f)
        smas = d.get("smas", [])
        results = []
        for i, sma in enumerate(smas):
            label = f"SMA{i + 1}"
            direction = sma.get("direction", "N/A")
            freq = sma.get("frequency_hz")
            freq_str = f"{freq} Hz" if freq else "N/A"
            results.append((label, direction, freq_str))
        return results
    except Exception:
        return []


def get_service_status():
    """Get status and recent log lines of all monitored services.
       Returns (active_list, inactive_list) where each item is (svc, status, logs)."""
    active = []
    inactive = []
    for svc in SERVICES:
        status = run_cmd(["systemctl", "is-active", svc])
        logs = ""
        if status == "active":
            logs = run_cmd(["journalctl", "-u", svc, "-n", str(LOG_LINES),
                            "--no-pager", "--output=short"])
            active.append((svc, status, logs))
        else:
            inactive.append((svc, status, logs))
    return active, inactive


def get_dpll_status():
    """Query DPLL channel states."""
    if not os.path.isfile(DPLLTOOL):
        return [("DPLL", "dplltool not found", "")]
    results = []
    for ch in [5, 6]:
        label = "Ch5 (Freq)" if ch == 5 else "Ch6 (Time)"
        state = run_cmd(["sudo", DPLLTOOL, "get-state", str(ch)])
        combo = run_cmd(["sudo", DPLLTOOL, "get-combo-slave", str(ch)])
        results.append((label, state, combo))
    return results


def get_ptp_role():
    """Read configured PTP role from config JSON."""
    try:
        if os.path.isfile(CONFIG_PATH):
            with open(CONFIG_PATH) as f:
                d = json.load(f)
            return d.get("ptp_role", "NONE").upper()
    except Exception:
        pass
    return "NONE"


def get_ptp_status():
    """Query PTP status via pmc. Returns a dict with role and parsed fields."""
    role = get_ptp_role()
    result = {"role": role}

    try:
        # ptp4l creates a UDS socket at /var/run/ptp4l.
        # Check both paths since /var/run may be a symlink to /run.
        sock = None
        for candidate in ["/var/run/ptp4l", "/run/ptp4l"]:
            if os.path.exists(candidate):
                sock = candidate
                break

        if not sock:
            if role == "GM":
                result["status"] = "ptp4l not running \u2014 waiting for DPLL + ts2phc convergence"
            elif role == "CLIENT":
                result["status"] = "ptp4l not running \u2014 ptp4l-client may not be started yet"
            else:
                result["status"] = "PTP not configured (role=NONE)"
            return result

        # Query port state
        port_out = run_cmd(["sudo", "pmc", "-u", "-s", sock, "-b", "0", "GET PORT_DATA_SET"])
        for line in port_out.split("\n"):
            line = line.strip()
            if "portState" in line:
                parts = line.split()
                if len(parts) >= 2:
                    result["port_state"] = parts[1]

        # Query time status
        out = run_cmd(["sudo", "pmc", "-u", "-s", sock, "-b", "0", "GET TIME_STATUS_NP"])
        for line in out.split("\n"):
            line = line.strip()
            if "master_offset" in line:
                parts = line.split()
                if len(parts) >= 2:
                    result["master_offset"] = parts[1]
            if "gmIdentity" in line:
                parts = line.split()
                if len(parts) >= 2:
                    result["gm_identity"] = parts[1]

        return result
    except Exception as e:
        result["status"] = f"Error: {e}"
        return result


def build_html():
    """Build the complete status HTML page."""
    hostname = esc(get_hostname())
    mac, ip = get_eth0_info()
    config = get_config_summary()
    active_svcs, inactive_svcs = get_service_status()
    dpll = get_dpll_status()
    ptp = get_ptp_status()
    smas = get_sma_config()

    # Active service rows with log tails
    active_svc_rows = ""
    for svc, status, logs in active_svcs:
        log_block = ""
        if logs:
            log_block = f'<pre class="logs">{esc(logs)}</pre>'
        active_svc_rows += f'''<tr>
            <td>🟢 {esc(svc)}</td>
            <td style="color:#4caf50;font-weight:bold">{esc(status)}</td>
        </tr>\n'''
        if log_block:
            active_svc_rows += f'<tr><td colspan="2">{log_block}</td></tr>\n'

    # Inactive service rows (no logs)
    inactive_svc_rows = ""
    for svc, status, logs in inactive_svcs:
        inactive_svc_rows += f'''<tr>
            <td style="color:#666">⚫ {esc(svc)}</td>
            <td style="color:#666">{esc(status)}</td>
        </tr>\n'''

    # DPLL rows
    dpll_rows = ""
    for label, state, combo in dpll:
        dpll_rows += f'<tr><td>{esc(label)}</td><td>{esc(state)}</td><td>{esc(combo)}</td></tr>\n'

    # PTP info — role-aware display
    ptp_role = ptp.get("role", "NONE")
    if "status" in ptp:
        ptp_block = f'<p>{esc(ptp.get("status"))}</p>'
    elif ptp_role == "GM":
        ptp_block = f"""
        <table>
            <tr><td>Role</td><td><strong>Grandmaster</strong></td></tr>
            <tr><td>Port State</td><td>{esc(ptp.get("port_state"))}</td></tr>
            <tr><td>GM Identity</td><td>{esc(ptp.get("gm_identity"))}</td></tr>
        </table>"""
    elif ptp_role == "CLIENT":
        ptp_block = f"""
        <table>
            <tr><td>Role</td><td><strong>Client</strong></td></tr>
            <tr><td>Port State</td><td>{esc(ptp.get("port_state"))}</td></tr>
            <tr><td>Master Offset</td><td>{esc(ptp.get("master_offset"))} ns</td></tr>
            <tr><td>GM Identity</td><td>{esc(ptp.get("gm_identity"))}</td></tr>
        </table>"""
    else:
        ptp_block = '<p>PTP not configured (role=NONE)</p>'

    # Config info
    if "error" in config:
        cfg_block = f'<p>Error: {esc(config["error"])}</p>'
    else:
        cfg_block = f"""
        <table>
            <tr><td>PTP Role</td><td><strong>{esc(config.get("ptp_role"))}</strong></td></tr>
            <tr><td>PTP Transport</td><td>{esc(config.get("ptp_transport"))}</td></tr>
            <tr><td>PTP Master IP</td><td>{esc(config.get("ptp_master_ip"))}</td></tr>
            <tr><td>GPS</td><td>Present={config.get("gps_present")} Role={esc(config.get("gps_role"))}</td></tr>
            <tr><td>CM4 Source</td><td>{config.get("cm4_used")}</td></tr>
            <tr><td>SyncE</td><td>{config.get("synce_used")}</td></tr>
            <tr><td>Network</td><td>{esc(config.get("network_mode"))} {esc(config.get("network_ip"))}</td></tr>
        </table>"""

    # SMA config
    sma_block = ""
    if smas:
        sma_rows = ""
        for label, direction, freq in smas:
            sma_rows += f'<tr><td>{esc(label)}</td><td>{esc(direction)}</td><td>{esc(freq)}</td></tr>\n'
        sma_block = f"""
    <div class="card">
        <h2>SMA Ports</h2>
        <table>
            <tr><th>Port</th><th>Direction</th><th>Frequency</th></tr>
            {sma_rows}
        </table>
    </div>"""

    page = f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta http-equiv="refresh" content="10">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Switchberry Status — {hostname}</title>
    <style>
        * {{ margin: 0; padding: 0; box-sizing: border-box; }}
        body {{
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;
            background: #1a1a2e; color: #e0e0e0; padding: 20px;
        }}
        h1 {{ color: #00d4ff; margin-bottom: 5px; font-size: 1.8em; }}
        h2 {{
            color: #7b8cde; margin: 20px 0 10px 0; font-size: 1.1em;
            border-bottom: 1px solid #333; padding-bottom: 5px;
        }}
        .subtitle {{ color: #888; font-size: 0.85em; margin-bottom: 15px; }}
        .card {{
            background: #16213e; border-radius: 8px; padding: 15px;
            margin-bottom: 15px; border: 1px solid #0f3460;
        }}
        table {{ border-collapse: collapse; width: 100%; }}
        th {{ text-align: left; padding: 4px 10px; color: #7b8cde; font-size: 0.8em;
             border-bottom: 1px solid #333; }}
        td {{ padding: 4px 10px; border-bottom: 1px solid #222; font-size: 0.9em; }}
        td:first-child {{ color: #aaa; white-space: nowrap; }}
        .identity {{ display: flex; gap: 30px; flex-wrap: wrap; }}
        .identity span {{ color: #aaa; font-size: 0.85em; }}
        .identity strong {{ color: #e0e0e0; }}
        .refresh {{ color: #555; font-size: 0.75em; margin-top: 10px; }}
        pre.logs {{
            background: #0d1b2a; color: #8892b0; padding: 8px; margin: 4px 0;
            border-radius: 4px; font-size: 0.75em; line-height: 1.4;
            overflow-x: auto; white-space: pre-wrap; word-break: break-all;
            border: 1px solid #1e3a5f;
        }}
    </style>
</head>
<body>
    <h1>🍓 Switchberry</h1>
    <p class="subtitle">Timing Appliance Status Dashboard</p>

    <div class="card">
        <h2>System Identity</h2>
        <div class="identity">
            <div><span>Hostname:</span> <strong>{hostname}</strong></div>
            <div><span>eth0 MAC:</span> <strong>{esc(mac)}</strong></div>
            <div><span>eth0 IP:</span> <strong>{esc(ip)}</strong></div>
        </div>
    </div>

    <div class="card">
        <h2>Active Configuration</h2>
        {cfg_block}
    </div>

    {sma_block}

    <div class="card">
        <h2>Active Services</h2>
        <table>{active_svc_rows}</table>
    </div>

    <div class="card">
        <h2>Inactive Services</h2>
        <table>{inactive_svc_rows}</table>
    </div>

    <div class="card">
        <h2>DPLL Hardware</h2>
        <table>
            <tr><th>Channel</th><th>State</th><th>Combo Slave</th></tr>
            {dpll_rows}
        </table>
    </div>

    <div class="card">
        <h2>PTP Status</h2>
        {ptp_block}
    </div>

    <p class="refresh">Auto-refreshes every 10 seconds</p>
</body>
</html>"""
    return page


class StatusHandler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/" or self.path == "/status":
            content = build_html()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(content.encode())))
            self.end_headers()
            self.wfile.write(content.encode())
        else:
            self.send_response(404)
            self.end_headers()
            self.wfile.write(b"Not Found")

    def log_message(self, format, *args):
        pass


def run_https_server(port, cert_file):
    """Run an HTTPS server on a separate port using a self-signed cert."""
    try:
        server = http.server.HTTPServer(("0.0.0.0", port), StatusHandler)
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.load_cert_chain(cert_file)
        server.socket = ctx.wrap_socket(server.socket, server_side=True)
        print(f"HTTPS server running on port {port}")
        server.serve_forever()
    except Exception as e:
        print(f"Warning: HTTPS server failed to start: {e}")


def main():
    parser = argparse.ArgumentParser(description="Switchberry Status Web Server")
    parser.add_argument("--port", "-p", type=int, default=DEFAULT_PORT, help="HTTP port")
    parser.add_argument("--https-port", type=int, default=DEFAULT_HTTPS_PORT, help="HTTPS port")
    parser.add_argument("--no-https", action="store_true", help="Disable HTTPS")
    args = parser.parse_args()

    # Start HTTPS in a background thread
    if not args.no_https:
        cert = ensure_self_signed_cert()
        if cert:
            t = threading.Thread(target=run_https_server, args=(args.https_port, cert),
                                 daemon=True)
            t.start()

    # Start HTTP (main thread)
    server = http.server.HTTPServer(("0.0.0.0", args.port), StatusHandler)
    print(f"HTTP server running on port {args.port}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down.")
        server.server_close()


if __name__ == "__main__":
    main()
