#!/bin/bash

# Usage: ./ksz_reg.sh <size> <address> [value]
# Example read:  ./ksz_reg.sh 8 0x1432
# Example write: ./ksz_reg.sh 16 0x1432 0xABCD

SYSFS_BASE="/sys/bus/spi/devices/spi0.0"

# Validate args
if [[ "$#" -lt 2 ]]; then
    echo "Usage: $0 <size:8|16|32> <reg_addr> [reg_val]"
    exit 1
fi

SIZE="$1"
ADDR="$2"
VAL="$3"

# Validate size
if [[ "$SIZE" != "8" && "$SIZE" != "16" && "$SIZE" != "32" ]]; then
    echo "Invalid size: $SIZE (must be 8, 16, or 32)"
    exit 1
fi

# Set register size
echo "$SIZE" | sudo tee "$SYSFS_BASE/reg_size" > /dev/null

# Set register address
echo "$ADDR" | sudo tee "$SYSFS_BASE/reg_addr" > /dev/null

if [[ -n "$VAL" ]]; then
    # Write value
    echo "$VAL" | sudo tee "$SYSFS_BASE/reg_val" > /dev/null
    echo "Wrote $VAL to $ADDR (size ${SIZE}-bit)"
else
    # Read value
    VALUE=$(cat "$SYSFS_BASE/reg_val")
    echo "Read from $ADDR (size ${SIZE}-bit): $VALUE"
fi

