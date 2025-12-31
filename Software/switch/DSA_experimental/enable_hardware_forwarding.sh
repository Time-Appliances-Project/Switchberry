#!/bin/bash

# Path to the register access script
KSZ_REG="./ksz_reg.sh"

# enable port vlan membership -> this is what's needed!
$KSZ_REG 32 0x1a04 0x5f
$KSZ_REG 32 0x2a04 0x5f
$KSZ_REG 32 0x3a04 0x5f
$KSZ_REG 32 0x4a04 0x5f
$KSZ_REG 32 0x5a04 0x5f
$KSZ_REG 32 0x6a04 0x5f
$KSZ_REG 32 0x7a04 0x5f

# Next theory, unicast learning control
$KSZ_REG 8 0x0311 0xc5 
exit 0



# One theory, Port ACL Access D register, enable forwarding
$KSZ_REG 8 0x160d 0x7f
$KSZ_REG 8 0x260d 0x7f
$KSZ_REG 8 0x360d 0x7f
$KSZ_REG 8 0x460d 0x7f
$KSZ_REG 8 0x560d 0x7f
$KSZ_REG 8 0x760d 0x7f



# Main item, enable vlan 1 to forward all ports except CPU (port 5 here)
$KSZ_REG 32 0x400 0x88000001
$KSZ_REG 32 0x404 0x5F
$KSZ_REG 32 0x408 0x5F
$KSZ_REG 16 0x40c 0x1
$KSZ_REG 8 0x40e 0x81
sleep 0.5
$KSZ_REG 8 0x40e 0x0

# Next theory, disable 802.1Q vlan enable
$KSZ_REG 8 0x310 0x65

# Spanning tree process, check receive and transmit enable bits

