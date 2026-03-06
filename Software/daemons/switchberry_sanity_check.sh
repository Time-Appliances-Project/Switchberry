#!/usr/bin/env bash
# switchberry_sanity_check.sh — Verify kernel config for Switchberry hardware.
#
# Checks that the required device-tree overlays are loaded and the expected
# device nodes exist.  Retries up to MAX_RETRIES times with a delay to handle
# boot race conditions (SPI/I2C devices may take a moment to probe).
# Exits non-zero (and logs to kernel/journal) on failure so that systemd can
# block dependent services from starting.
#
# Required overlays (from /boot/firmware/config.txt):
#   dtoverlay=switchberrytc    -> I2C-9 GPIO expander (TCA6424) at 0x23
#   dtoverlay=spi0-1cs         -> /dev/spidev0.0 (KSZ9567 switch SPI)
#   dtoverlay=uart5,...        -> /dev/ttyAMA5 (GPS UART)
#   (switchberrytc also provides /dev/spidev7.0 via SPI-GPIO bitbang for DPLL)

set -euo pipefail

TAG="switchberry-sanity"
MAX_RETRIES="${MAX_RETRIES:-10}"
RETRY_DELAY="${RETRY_DELAY:-2}"

# ── Device nodes to check ──
declare -A CHECKS=(
    ["/dev/spidev7.0"]="DPLL SPI (switchberrytc overlay)"
    ["/dev/ttyAMA5"]="GPS UART (uart5 overlay)"
    ["/sys/bus/i2c/devices/9-0023"]="TCA6424 GPIO expander (switchberrytc overlay, I2C-9 @ 0x23)"
)

echo "[$TAG] Checking Switchberry hardware prerequisites..."

for attempt in $(seq 1 "$MAX_RETRIES"); do
    FAIL=0
    MISSING=""

    for path in "${!CHECKS[@]}"; do
        desc="${CHECKS[$path]}"
        if [[ -e "$path" ]]; then
            echo "  [OK]   $path  ($desc)"
        else
            echo "  [WAIT] $path  ($desc)"
            FAIL=1
            MISSING="$MISSING  $path ($desc)\n"
        fi
    done

    if [[ "$FAIL" -eq 0 ]]; then
        echo "[$TAG] All checks passed."
        exit 0
    fi

    if [[ "$attempt" -lt "$MAX_RETRIES" ]]; then
        echo "[$TAG] Attempt $attempt/$MAX_RETRIES: some devices not ready, retrying in ${RETRY_DELAY}s..."
        sleep "$RETRY_DELAY"
    fi
done

# All retries exhausted
MSG="Switchberry kernel configuration is not correct. Please check /boot/firmware/config.txt and ensure the required overlays are present."
echo ""
echo "  *** SANITY CHECK FAILED (after $MAX_RETRIES attempts) ***"
echo "  $MSG"
echo ""
echo "  Missing devices:"
echo -e "$MISSING"
echo "  Expected config.txt entries:"
echo "    dtoverlay=switchberrytc"
echo "    dtoverlay=spi0-1cs"
echo "    dtoverlay=uart5,txd5_pin=12,rxd5_pin=13"
echo ""
logger -p kern.err -t "$TAG" "$MSG"
exit 1
