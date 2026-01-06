#!/usr/bin/env bash
set -euo pipefail

CONFIG=/etc/startup-dpll.json

echo "[switchberry-ptp-role] Reading PTP role from ${CONFIG}"

if [ ! -f "$CONFIG" ]; then
    echo "[switchberry-ptp-role] ERROR: config file not found"
    exit 1
fi

ROLE=$(python3 - <<EOF
import json, sys
cfg = json.load(open("${CONFIG}"))
print(cfg.get("ptp_role", "NONE"))
EOF
)

echo "[switchberry-ptp-role] Detected role: ${ROLE}"

case "${ROLE}" in
    GM)
        echo "[switchberry-ptp-role] Configuring as GRANDMASTER"

        # Start GM stack
        systemctl start ts2phc-switchberry.service || true
        systemctl start ptp4l-switchberry-gm.service || true

        # Optional cleanup: stop client stack if running
        systemctl stop ptp4l-switchberry-client.service 2>/dev/null || true
        ;;

    CLIENT)
        echo "[switchberry-ptp-role] Configuring as CLIENT"

        # Start client stack (you’ll define these separately)
        systemctl start ptp4l-switchberry-client.service || true

        # Optional cleanup: stop GM stack if running
        systemctl stop ts2phc-switchberry.service 2>/dev/null || true
        systemctl stop ptp4l-switchberry-gm.service 2>/dev/null || true
        ;;

    NONE|""|*)
        echo "[switchberry-ptp-role] PTP role NONE/unknown; no PTP services started"
        # Optional: stop everything
        systemctl stop ts2phc-switchberry.service 2>/dev/null || true
        systemctl stop ptp4l-switchberry-gm.service 2>/dev/null || true
        systemctl stop ptp4l-switchberry-client.service 2>/dev/null || true
        ;;
esac

echo "[switchberry-ptp-role] Done."

