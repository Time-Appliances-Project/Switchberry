#!/usr/bin/env bash
# sb-status.sh
# Displays the current status of Switchberry services and hardware synchronization.

SOFTWARE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DPLLTOOL="$SOFTWARE_DIR/clockmatrix/dpll/dplltool"

echo "========================================"
echo "    Switchberry System Status           "
echo "========================================"

# 1. Configuration (JSON)
echo "[Active Config]"
JSON_FILE="/etc/startup-dpll.json"
if [[ -f "$JSON_FILE" ]]; then
    # Use python3 to parse JSON summary since jq might not be present
    python3 -c "
import json
try:
    with open('$JSON_FILE') as f:
        d = json.load(f)
    print(f\"  PTP Role: {d.get('ptp_role', 'Unknown')}\")
    print(f\"  GPS:      Pres={d.get('gps',{}).get('present')} Role={d.get('gps',{}).get('role')}\")
    print(f\"  CM4:      Used={d.get('cm4',{}).get('used_as_source')} Role={d.get('cm4',{}).get('role')}\")
    print(f\"  SyncE:    Used={d.get('synce',{}).get('used_as_source')}\")
except Exception as e:
    print(f\"  Error parsing JSON: {e}\")
"
else
    echo "  [!] $JSON_FILE not found"
fi
echo

# 2. Services
echo "[Services]"
SERVICES=(
    "ptp4l-switchberry-client.service"
    "ptp4l-switchberry-gm.service"
    "switchberry-cm4-pps-monitor.service"
    "switchberry-dpll-monitor.service"
    "switchberry-ptp-role.service"
    "ts2phc-switchberry.service"
)

for svc in "${SERVICES[@]}"; do
    STATUS=$(systemctl is-active "$svc" 2>/dev/null || echo "inactive")
    if [[ "$STATUS" == "active" ]]; then
        echo "  [OK] $svc"
        # Print last 3 log lines
        journalctl -u "$svc" -n 3 --no-pager | sed 's/^/      /'
    else
        echo "  [STOPPED] $svc ($STATUS)"
    fi
done
echo



# 4. DPLL Hardware
echo "[DPLL Hardware]"
if [[ -x "$DPLLTOOL" ]]; then
    # Helper to get state with sudo
    get_dpll_state() {
        sudo "$DPLLTOOL" get-state "$1" 2>/dev/null || echo "Error"
    }
    
    # Query Channel 5 (Frequency) and Channel 6 (Time)
    ST5=$(get_dpll_state 5)
    ST6=$(get_dpll_state 6)
    
    # Query Combo Slave Status (New Feature)
    get_combo_status() {
        sudo "$DPLLTOOL" get-combo-slave "$1" 2>/dev/null || echo "Unknown"
    }
    
    CS5=$(get_combo_status 5)
    CS6=$(get_combo_status 6)
    
    echo "  Channel 5 (Freq): $ST5"
    echo "    $CS5"
    echo "  Channel 6 (Time): $ST6"
    echo "    $CS6"
else
    echo "  [!] dplltool not found (cannot query hardware)"
fi
echo

# 4. PTP Status (pmc)
echo "[PTP Status]"
if command -v pmc >/dev/null 2>&1; then
    # Scan for sockets: /var/run/ptp4l*
    SOCKETS=$(sudo find /var/run -name "ptp4l*" 2>/dev/null)
    
    # Fallback: manually check common paths if find failed
    if [[ -z "$SOCKETS" ]]; then
        for p in /var/run/ptp4l /var/run/ptp4lro; do
            if [[ -S "$p" ]]; then
                SOCKETS="$SOCKETS $p"
            fi
        done
    fi
    
    FOUND_STATUS=0
    
    if [[ -z "$SOCKETS" ]]; then
        echo "  [!] No PTP sockets found in /var/run (ptp4l not running?)"
    else
        for SOCK in $SOCKETS; do
            # Try probing this socket on domain 0 (default) or others if needed
            # We use -b 0 mostly, but some profiles use other domains.
            # However, with UDS (-s), the domain usually doesn't matter for local query unless
            # the ptp4l instance enforces it.
            
            # Try to get status
            PMC_CMD="sudo pmc -u -s $SOCK -b 0 'GET TIME_STATUS_NP'"
            OUT=$(eval "$PMC_CMD" 2>/dev/null)
            
            if [[ -n "$OUT" ]]; then
                echo "  Socket:        $SOCK"
                
                # Extract Domain Number from the output if available (TIME_STATUS_NP includes domainNumber?)
                # Actually TIME_STATUS_NP doesn't have domainNumber.
                # But we can assume it worked.
                
                OFFSET=$(echo "$OUT" | grep "master_offset" | awk '{print $2}')
                GM_ID=$(echo "$OUT" | grep "gmIdentity" | awk '{print $2}')
                echo "  Master Offset: ${OFFSET:-Unknown} ns"
                echo "  GM Identity:   ${GM_ID:-Unknown}"
                
                # Check port state
                PORT_OUT=$(sudo pmc -u -s "$SOCK" -b 0 'GET PORT_DATA_SET' 2>/dev/null)
                PORT_STATE=$(echo "$PORT_OUT" | grep "portState" | awk '{print $2}')
                echo "  Port State:    ${PORT_STATE:-Unknown}"
                
                FOUND_STATUS=1
                break # Stop after finding one active PTP instance (usually only one matters)
            fi
        done
        
        if [[ "$FOUND_STATUS" -eq 0 ]]; then
             echo "  [!] Found sockets but pmc query failed ($SOCKETS)"
        fi
    fi
else
    echo "  [!] pmc command not found"
fi

echo "========================================"
