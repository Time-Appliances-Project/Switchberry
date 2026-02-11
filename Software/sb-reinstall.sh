#!/usr/bin/env bash
set -euo pipefail

# sb-reinstall.sh
# Automates the full re-installation workflow for Switchberry software:
# 1. Reset DPLL hardware config (force reload from EEPROM/Default)
# 2. Stop running services
# 3. Rebuild and install all software (using install_all.sh)
# 4. Restart services

SOFTWARE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DPLLTOOL="$SOFTWARE_DIR/clockmatrix/dpll/dplltool"

echo "========================================"
echo "    Switchberry Reinstall Automation    "
echo "========================================"

# 1. Reset DPLL
# 0xc012 = DPLL_MODE register. 0x5a = Trigger EEPROM Reload / Reset?
# User specified: dplltool --write 0xc012 0x5a
if [[ -x "$DPLLTOOL" ]]; then
    echo "[1/4] Resetting DPLL configuration..."
    "$DPLLTOOL" --write 0xc012 0x5a
    echo "DPLL reset command sent."
    sleep 1
else
    echo "[1/4] Warning: dplltool not found at $DPLLTOOL. Skipping DPLL reset."
fi

# 2. Stop Services
echo "[2/4] Stopping services..."
if [[ -d "$SOFTWARE_DIR/daemons" ]]; then
    (cd "$SOFTWARE_DIR/daemons" && sudo make stop) || echo "Warning: 'make stop' returned error (ignoring)."
else
    echo "Warning: daemons directory not found."
fi

# 3. Install All
echo "[3/4] Building and Installing software..."
if [[ -f "$SOFTWARE_DIR/install_all.sh" ]]; then
    bash "$SOFTWARE_DIR/install_all.sh" -y
else
    echo "Error: install_all.sh not found!"
    exit 1
fi

# 4. Restart Services
echo "[4/4] Restarting services..."
if [[ -d "$SOFTWARE_DIR/daemons" ]]; then
    (cd "$SOFTWARE_DIR/daemons" && sudo make restart)
else
    echo "Warning: daemons directory not found."
fi

echo "========================================"
echo "           Reinstall Complete           "
echo "========================================"
