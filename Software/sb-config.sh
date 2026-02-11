#!/usr/bin/env bash
set -euo pipefail

# sb-config.sh
# Automates the configuration workflow:
# 1. Runs the config.py wizard (interactive)
# 2. Checks if successful
# 3. Copies dpll-config.json to /etc/startup-dpll.json

SOFTWARE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_DIR="$SOFTWARE_DIR/clockmatrix"
CONFIG_PY="config.py"
JSON_NAME="dpll-config.json"
JSON_DEST="/etc/startup-dpll.json"

echo "========================================"
echo "    Switchberry Configuration Wizard    "
echo "========================================"

if [[ ! -f "$CONFIG_DIR/$CONFIG_PY" ]]; then
    echo "Error: config.py not found in $CONFIG_DIR"
    exit 1
fi

# Run wizard
echo ">>> Launching Wizard..."
(cd "$CONFIG_DIR" && python3 "$CONFIG_PY" wizard) || { echo "Wizard failed or aborted."; exit 1; }

# Verify output
JSON_SRC="$CONFIG_DIR/$JSON_NAME"
if [[ ! -f "$JSON_SRC" ]]; then
    echo "Error: $JSON_NAME was not generated. Did you complete the wizard?"
    exit 1
fi

# Install
echo ">>> Installing configuration..."
echo "    Source: $JSON_SRC"
echo "    Dest:   $JSON_DEST"

if sudo cp "$JSON_SRC" "$JSON_DEST"; then
    echo ">>> Success! Configuration installed to $JSON_DEST"
    echo "    (Previous config overwritten)"
else
    echo ">>> Error: Failed to copy configuration (check permissions)."
    exit 1
fi

echo "========================================"
echo "           Configuration Done           "
echo "========================================"
echo "Tip: Run ./sb-reinstall.sh or restart services to apply changes."
