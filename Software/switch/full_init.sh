#!/usr/bin/env bash
set -euo pipefail



gpioset -t 0 -c gpiochip0 7=0
gpioset -t 0 -c gpiochip0 7=1
sudo ./phy_init.sh
sudo ./switch_cli.sh switch init



CFG="/etc/startup-dpll.json"
if jq -e '.synce.used_as_source == true' "$CFG" >/dev/null 2>&1; then
    PORT="$(jq -r '.synce.recover_port // empty' "$CFG")"

    if [[ "$PORT" =~ ^[1-5]$ ]]; then
        echo "SyncE enabled, recover_port=$PORT"
	sudo ./switch_cli.sh switch synce-recover "$PORT"
    else
        echo "ERROR: SyncE enabled but recover_port is missing/invalid in $CFG (got '$PORT')" >&2
        exit 1
    fi
else
    echo "SyncE not enabled in config"
fi
