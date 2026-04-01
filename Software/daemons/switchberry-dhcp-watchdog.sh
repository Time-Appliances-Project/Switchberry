#!/usr/bin/env bash
# switchberry-dhcp-watchdog.sh
#
# Periodically checks if eth0 has an IPv4 address when configured for DHCP.
# If not, retries "nmcli con up" to trigger a new DHCP request.
#
# This is needed because eth0 is always link-UP (connected to the internal
# KSZ9567 switch), so NetworkManager sees carrier even when no RJ45 cable
# is plugged into the front panel. If DHCP fails at boot, NM may not retry
# on its own since carrier never transitions.

CONFIG_FILE="/etc/startup-dpll.json"
RETRY_INTERVAL=30   # seconds between checks when no address
STABLE_INTERVAL=60  # seconds between checks when address is present

# Check if configured for DHCP
get_network_mode() {
    python3 -c "
import json, sys
try:
    with open('$CONFIG_FILE') as f:
        d = json.load(f)
    print(d.get('network', {}).get('mode', 'DHCP').upper())
except Exception:
    print('DHCP')
" 2>/dev/null
}

# Find the NM connection name for eth0
get_con_name() {
    local name
    name=$(nmcli -t -f NAME,DEVICE con show 2>/dev/null | grep ':eth0$' | head -1 | cut -d: -f1)
    if [ -z "$name" ]; then
        name="Wired connection 1"
    fi
    echo "$name"
}

# Check if eth0 has an IPv4 address
has_ipv4() {
    ip -4 addr show eth0 2>/dev/null | grep -q 'inet '
}

# --- Main ---

MODE=$(get_network_mode)
if [ "$MODE" != "DHCP" ]; then
    echo "Network mode is $MODE (not DHCP). Watchdog not needed. Exiting."
    exit 0
fi

CON_NAME=$(get_con_name)
echo "DHCP watchdog started. Connection: '$CON_NAME', checking every ${RETRY_INTERVAL}s"

# Handle SIGTERM gracefully
trap 'echo "DHCP watchdog stopping."; exit 0' SIGTERM SIGINT

while true; do
    if has_ipv4; then
        sleep "$STABLE_INTERVAL" &
        wait $!
    else
        echo "eth0 has no IPv4 address. Requesting DHCP via nmcli con up '$CON_NAME'..."
        nmcli con up "$CON_NAME" 2>&1 || true
        sleep "$RETRY_INTERVAL" &
        wait $!
    fi
done
