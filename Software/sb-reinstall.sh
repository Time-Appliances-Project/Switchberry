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

# ── Optional: run in background to survive SSH disconnects ──
# Use --background flag if SSH'd through the Ethernet switch (DPLL reset drops link).
# Default: run directly in the foreground.
if [[ "${1:-}" == "--background" ]]; then
    shift
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
    nohup sudo bash "${BASH_SOURCE[0]}" "$@" > "$LOG_FILE" 2>&1 &
    BG_PID=$!
    echo "$BG_PID" > "$LOCK_FILE"
    disown
    exit 0
fi

# ── Background worker (runs detached) ──
DPLLTOOL="$SOFTWARE_DIR/clockmatrix/dpll/dplltool"

echo "========================================"
echo "    Switchberry Reinstall (background)  "
echo "    $(date)                              "
echo "========================================"

# 1. Reset DPLL
# 0xc012 = DPLL_MODE register. 0x5a = Trigger EEPROM Reload / Reset
# WARNING: This disrupts the KSZ9567 clock tree and will break Ethernet.
if [[ -x "$DPLLTOOL" ]]; then
    echo "[1/6] Resetting DPLL configuration..."
    "$DPLLTOOL" --write 0xc012 0x5a
    echo "DPLL reset command sent."
    sleep 1
else
    echo "[1/6] Warning: dplltool not found at $DPLLTOOL. Skipping DPLL reset."
fi

# 2. Stop Services
echo "[2/6] Stopping services..."
if [[ -d "$SOFTWARE_DIR/daemons" ]]; then
    (cd "$SOFTWARE_DIR/daemons" && sudo make stop) || echo "Warning: 'make stop' returned error (ignoring)."
else
    echo "Warning: daemons directory not found."
fi

# 3. Install All
echo "[3/6] Building and Installing software..."
if [[ -f "$SOFTWARE_DIR/install_all.sh" ]]; then
    bash "$SOFTWARE_DIR/install_all.sh" -y
else
    echo "Error: install_all.sh not found!"
    exit 1
fi

# 4. Generate PTP config files
echo "[4/6] Generating PTP config files..."
CONF_JSON="/etc/startup-dpll.json"
if [[ -f "$SOFTWARE_DIR/clockmatrix/generate_ptp_conf.py" ]] && [[ -f "$CONF_JSON" ]]; then
    sudo python3 "$SOFTWARE_DIR/clockmatrix/generate_ptp_conf.py" -c "$CONF_JSON"
else
    echo "Warning: generate_ptp_conf.py or config not found, skipping."
fi

# 5. Apply network (eth0) configuration
echo "[5/6] Applying network configuration..."
if [[ -f "$SOFTWARE_DIR/clockmatrix/apply_network.py" ]] && [[ -f "$CONF_JSON" ]]; then
    sudo python3 "$SOFTWARE_DIR/clockmatrix/apply_network.py" -c "$CONF_JSON"
else
    echo "Warning: apply_network.py or config not found, skipping."
fi

# 6. Restart Services
echo "[6/6] Restarting services..."
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

