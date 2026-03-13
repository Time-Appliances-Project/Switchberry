#!/usr/bin/env bash
set -euo pipefail

# sb-reinstall.sh
# Automates the full re-installation workflow for Switchberry software.
#
# IMPORTANT: Step 1 resets the DPLL, which disrupts the KSZ9567 clock tree.
# If you are SSH'd into the CM4 through the switch, your session WILL drop.
# To avoid interrupting the reinstall, this script automatically runs the
# real work in the background (via nohup) and logs progress to a file.
# The script is safe to run over SSH — it will survive the disconnect.

SOFTWARE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_FILE="/tmp/sb-reinstall.log"
LOCK_FILE="/tmp/sb-reinstall.lock"

# ── Auto-detect SSH over eth0 and run in background ──
# If the user is SSH'd through the Ethernet switch, the DPLL reset will break
# the link and kill the SSH session. Detect this and auto-switch to background.
ssh_over_eth0() {
    # Check if we're in an SSH session that came in through eth0.
    # SSH_CONNECTION = "client_ip client_port server_ip server_port"
    # Field 3 (server_ip) is the local IP that accepted the connection.
    # We check which interface owns that IP.
    if [[ -n "${SSH_CONNECTION:-}" ]]; then
        local server_ip
        server_ip="$(echo "$SSH_CONNECTION" | awk '{print $3}')"
        # Check if eth0 owns this IP
        if ip -4 addr show eth0 2>/dev/null | grep -q "inet ${server_ip}/"; then
            return 0
        fi
    fi
    return 1
}

BACKGROUND=0
if [[ "${1:-}" == "--background" ]]; then
    BACKGROUND=1
    shift
elif ssh_over_eth0; then
    echo "Detected SSH connection over eth0 — switching to background mode automatically."
    BACKGROUND=1
fi

if [[ "$BACKGROUND" -eq 1 ]]; then
    # Clean up stale log file (may be owned by root from a previous run)
    sudo rm -f "$LOG_FILE"
    touch "$LOG_FILE"

    # Prevent double-spawn: check for an existing background run
    if [[ -f "$LOCK_FILE" ]] && kill -0 "$(cat "$LOCK_FILE" 2>/dev/null)" 2>/dev/null; then
        echo "Error: A reinstall is already running (PID $(cat "$LOCK_FILE"))."
        echo "  Monitor: tail -f $LOG_FILE"
        exit 1
    fi

    echo "========================================"
    echo "    Switchberry Reinstall Automation    "
    echo "========================================"
    echo ""
    echo "Reinstall is running in the background so it survives SSH disconnects."
    echo "  Log file : $LOG_FILE"
    echo "  Monitor  : tail -f $LOG_FILE"
    echo ""
    echo "NOTE: If you are connected via the Ethernet switch, your SSH session"
    echo "      will drop when the DPLL resets. This is normal — the reinstall"
    echo "      will continue in the background. Reconnect after ~30 seconds."
    echo ""
    NOHUP_LAUNCHED=1 nohup sudo -E bash "${BASH_SOURCE[0]}" "$@" > "$LOG_FILE" 2>&1 &
    BG_PID=$!
    echo "$BG_PID" > "$LOCK_FILE"
    disown
    exit 0
fi

# ── Worker (runs either in foreground or detached via nohup) ──
DPLLTOOL="$SOFTWARE_DIR/clockmatrix/dpll/dplltool"

RUN_MODE="foreground"
if [[ -n "${NOHUP_LAUNCHED:-}" ]]; then RUN_MODE="background"; fi

echo "========================================"
echo "    Switchberry Reinstall ($RUN_MODE)   "
echo "    $(date)                              "
echo "========================================"

# 0. Fix script permissions + tell git to ignore permission changes
echo "[0/7] Fixing script permissions..."
find "$SOFTWARE_DIR" -name '*.sh' -exec chmod +x {} +
find "$SOFTWARE_DIR" -name '*.py' -exec chmod +x {} +
git -C "$SOFTWARE_DIR" config core.fileMode false 2>/dev/null || true

# 1. Reset DPLL
# 0xc012 = DPLL_MODE register. 0x5a = Trigger EEPROM Reload / Reset
# WARNING: This disrupts the KSZ9567 clock tree and will break Ethernet.
if [[ -x "$DPLLTOOL" ]]; then
    echo "[1/7] Resetting DPLL configuration..."
    "$DPLLTOOL" --write 0xc012 0x5a
    echo "DPLL reset command sent."
    sleep 1
else
    echo "[1/7] Warning: dplltool not found at $DPLLTOOL. Skipping DPLL reset."
fi

# 2. Stop Services
echo "[2/8] Stopping services..."
if [[ -d "$SOFTWARE_DIR/daemons" ]]; then
    (cd "$SOFTWARE_DIR/daemons" && sudo make stop) || echo "Warning: 'make stop' returned error (ignoring)."
else
    echo "Warning: daemons directory not found."
fi

# 2b. Disable stock chrony (conflicts with switchberry-chrony)
echo "     Disabling stock chronyd (if present)..."
sudo systemctl disable --now chronyd.service 2>/dev/null || true
sudo systemctl disable --now chrony.service 2>/dev/null || true

# 3. Flush journal logs for Switchberry services
# Clears old log entries so the web dashboard only shows logs from this install.
echo "[3/7] Flushing Switchberry service logs..."
sudo journalctl --rotate
for svc in switchberry-sanity switchberry-apply-timing switchberry-apply-network \
           switchberry-full-init switchberry-dpll-monitor switchberry-ptp-role \
           switchberry-status-web switchberry-cm4-pps-monitor \
           switchberry-phc2sys switchberry-chrony switchberry-dhcp-watchdog \
           ts2phc-switchberry ptp4l-switchberry-gm ptp4l-switchberry-client; do
    sudo journalctl --vacuum-time=1s -u "${svc}.service" 2>/dev/null || true
done

# 4. Install All
echo "[4/7] Building and Installing software..."
if [[ -f "$SOFTWARE_DIR/install_all.sh" ]]; then
    bash "$SOFTWARE_DIR/install_all.sh" -y
else
    echo "Error: install_all.sh not found!"
    exit 1
fi

# 5. Generate PTP config files
echo "[5/7] Generating PTP config files..."
CONF_JSON="/etc/startup-dpll.json"
if [[ -f "$SOFTWARE_DIR/clockmatrix/generate_ptp_conf.py" ]] && [[ -f "$CONF_JSON" ]]; then
    sudo python3 "$SOFTWARE_DIR/clockmatrix/generate_ptp_conf.py" -c "$CONF_JSON"
else
    echo "Warning: generate_ptp_conf.py or config not found, skipping."
fi

# 6. Apply network (eth0) configuration
echo "[6/7] Applying network configuration..."
if [[ -f "$SOFTWARE_DIR/clockmatrix/apply_network.py" ]] && [[ -f "$CONF_JSON" ]]; then
    sudo python3 "$SOFTWARE_DIR/clockmatrix/apply_network.py" -c "$CONF_JSON"
else
    echo "Warning: apply_network.py or config not found, skipping."
fi

# 7. Restart Services
echo "[7/7] Restarting services..."
if [[ -d "$SOFTWARE_DIR/daemons" ]]; then
    (cd "$SOFTWARE_DIR/daemons" && sudo make restart && sudo make enable)
else
    echo "Warning: daemons directory not found."
fi

echo "========================================"
echo "    Reinstall Complete — $(date)        "
echo "========================================"

# Clean up lock file
rm -f "$LOCK_FILE"

