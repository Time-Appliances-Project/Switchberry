#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/../ksz9567_spi.sh"


synce_recover() {
    local port="$1"
    local logical
    local reg_val

    if [[ -z "$port" ]]; then
        echo "Usage: $0 switch synce-recover <1-5>"
        return 1
    fi

    if ! [[ "$port" =~ ^[1-5]$ ]]; then
        echo "ERROR: Port must be 1-5 (front panel). Got: $port"
        return 1
    fi

    logical="$(physical_to_logical "$port")" || return 1

    # Per your rule:
    #   reg_val = (logical_port << 2) + (1<<1)
    reg_val=$(( (logical << 2) + (1 << 1) ))

    printf "Configuring SyncE recovery: physical %s -> logical %s; write 0x103 = 0x%X (%d)\n" \
        "$port" "$logical" "$reg_val" "$reg_val"

    # Perform the actual register write
    # (Assumes you already have a function like: ksz9567_spi_write <reg> <val>)
    ksz9567_spi_write 0x103 "$reg_val"

    return 0
}


switch_init() {
	echo "Initializing KSZ9567 switch..."

	#echo "RGMII Port 6, control register 0 = $(ksz9567_spi_read 0x6300)"
	#echo "RGMII Port 6, control register 1 = $(ksz9567_spi_read 0x6301)"

	#exit

	echo "Enabling RGMII skew"
	ksz9567_spi_write 0x6301 0x18


	echo "Handling BASE-T errata and enable ports"
	for (( i=0; i<5; i++ )); do
		echo "Port $i"
		# disable auto-neg, force 100M duplex, see errata items 1 / 2 / 4 / 
		ksz9567_spi_write16 $(ksz9567_get_port_register $i 0x100) 0x2100 
		
		# phy access is indirect, use different function

		# errata 1
		ksz9567_phy_mmd_write $i 0x1 0x6f 0xdd0b
		ksz9567_phy_mmd_write $i 0x1 0x8f 0x6032
		ksz9567_phy_mmd_write $i 0x1 0x9d 0x248c
		ksz9567_phy_mmd_write $i 0x1 0x75 0x0060
		ksz9567_phy_mmd_write $i 0x1 0xd3 0x7777
		ksz9567_phy_mmd_write $i 0x1c 0x06 0x3008
		ksz9567_phy_mmd_write $i 0x1c 0x08 0x2000
	
		# errata 2
		ksz9567_phy_mmd_write $i 0x1c 0x04 0x00d0
	
		# errata 4
		ksz9567_phy_mmd_write $i 0x7 0x3c 0x0
	
		# restart auto neg
		ksz9567_spi_write16 $(ksz9567_get_port_register $i 0x100) 0x1300
	done
	



	echo "Wait after enabling ports"
	sleep 2

	echo "setting sgmii port as host port , debug"
	ksz9567_spi_write 0x7020 0x4

	echo "Enable PTP transparent clock"
	ksz9567_spi_write16 0x500 0x8002 # THIS WORKED FOR SINGLE SWITCH TC
	ksz9567_spi_write16 0x514 0x79
	ksz9567_spi_write16 0x516 0x1c04 # THIS WORKED FOR SINGLE SWITCH TC
	#ksz9567_spi_write16 0x516 0x1804 # THIS WORKED FOR SINGLE SWITCH TC


	echo "Add PTP calibration determined experimentally"
	for (( i=0; i<5; i++ )); do
		ksz9567_spi_write16 $(ksz9567_get_port_register $i 0xc00) 0x1a9
		ksz9567_spi_write16 $(ksz9567_get_port_register $i 0xc02) 0xf3
	done
	


	echo "Enable unicast learning"
	# global setting, should be there be re-enable, enable unicast learning control
	ksz9567_spi_write 0x0311 0xD # SHOULD BE D , experiment

	echo "Disable sync-e output by default"
	ksz9567_spi_write 0x103 0x0

	echo "Enable small packets"
	ksz9567_spi_write 0x331 0xF1


	exit


        echo "Enable unknown unicast forward"
        ksz9567_spi_write32 0x320 0x8000003f

        echo "Enable unknown multicast forward to all ports"
        ksz9567_spi_write32 0x324 0x8000003f

        echo "Enable unknown VID forward to all ports"
        ksz9567_spi_write32 0x328 0x8000003f

	echo "Enabling port vlan membership, all forwarding should be through ACL config"
	for (( i=0; i<6; i++ )); do
		ksz9567_spi_write $(ksz9567_get_port_register $i 0xa04) 0x3f
	done

	echo "Force PTP Event message priority"
	ksz9567_spi_write 0x37c 0x80
	ksz9567_spi_write 0x37d 0x80



	exit






	###################### INGRESS PRIORITY STUFF

	echo "Setup ingress priority mapping"
	# Map all ingress priorities to zero, flat priority even though 4 queues
	for (( i=0; i<6; i++ )); do
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x801) 0x1
		ksz9567_spi_write32 $(ksz9567_get_port_register $i 0x808) 0x0

		# ingress rate limit
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x403) 0x2c
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x410) 0x0
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x411) 0x0
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x412) 0x0
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x413) 0x0
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x414) 0x0
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x415) 0x0
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x416) 0x0
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x417) 0x0
	done

	###################### EGRESS PRIORITY STUFF

	echo "Setting all ports 4 egress queue, debug"
	for (( i=0; i<6; i++ )); do
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x020) 0x2

		# egress rate limit
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x420) 0x0
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x421) 0x0
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x422) 0x0
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x423) 0x0

		for (( j=0; j < 4; j++ )); do
			ksz9567_spi_write32 $(ksz9567_get_port_register $i 0x900) $j #indirect queue index
			ksz9567_spi_write $(ksz9567_get_port_register $i 0x914) 0x0 #strict priority
			ksz9567_spi_write $(ksz9567_get_port_register $i 0x915) 0x1
			ksz9567_spi_write16 $(ksz9567_get_port_register $i 0x916) 0x8000 # shaper stuff
			ksz9567_spi_write16 $(ksz9567_get_port_register $i 0x918) 0x0
			ksz9567_spi_write16 $(ksz9567_get_port_register $i 0x91A) 0x8000
		done

		ksz9567_spi_write $(ksz9567_get_port_register $i 0x920) 0x1
	done
	ksz9567_spi_write 0x335 0x8
	ksz9567_spi_write32 0x390 0xc2





	exit






	echo "Using ACL to remap priority and port forwarding"
	# ACL description on page 44 to page 50
	# ACL registers start on page 167
	# ACL (Access Control List) can be created for each port 
	# Created as an ordered list up to 16 rules, looks for match condition
	# Three parts, matching rules, action rules, processing entries
	#  1. Matching rule specifies what comparison test shall be performed on each packet
	#  2. Action rule specifies forwarding action if matching test succeeds
	#
	# Basically, can have multiple tests (Matching rules) , then perform a single action
	# FRN = first rule number, 0 to 15, pointer to action rule entry
	# In my case, I want to override priority and forwarding map of every packet
	# so 1 action rule, 1 matching rule, 1 processing field entry
	# page 45-46 has good description
	for (( i=0; i<6; i++ )); do
		echo "Init ACL Port $i"

		# Enable ACL
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x803) 0x6

		# first rule number , zero
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x600) 0x0

		# match criteria
		# Mode 0x601[5:4] = 2'b01 for layer 2 MAC header
		# ENB (Enable) 0x601[3:2] = 2'b01 , comparison on ethertype
		# Source/Destination 0x601[1] = 0, doesnt matter
		# EQ 0x601[0] = 1'b0, match if compared values not equal
		# Layer 2, Ethertype, compare != 0
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x601) 0x14

		# mac address, not used
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x602) 0x0
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x603) 0x0
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x604) 0x0
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x605) 0x0
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x606) 0x0
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x607) 0x0

		# ethertype, zero
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x608) 0x0
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x609) 0x0

		# always change priority to zero
		# 0x60A[7:6] = Priority Mode = 2'b11 
		# 0x60A[5:3] = Priority to set 
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x60a) 0xc0
			
		# map mode 0x60b[6:5] = 2'b10, AND this map with map from port	
		#		debug, 2'b11, replace with this map
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x60b) 0x60
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x60d) 0x3f
		#ksz9567_spi_write $(ksz9567_get_port_register $i 0x60d) 0x0 # block all forwarding, debug test

		# rule set zero, BITMASK
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x60e) 0x0
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x60f) 0x1
			
		# enable all registers write screw it
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x610) 0xff
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x611) 0xff

		# write ACL Index 0	
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x612) 0x10


		# not sure, but reinit the whole ACL table for the port
		for (( j=1; j<16; j++ )); do
			ksz9567_spi_write $(ksz9567_get_port_register $i 0x601) 0x0 # matching rule disabled
			ksz9567_spi_write $(ksz9567_get_port_register $i 0x610) 0xff
			ksz9567_spi_write $(ksz9567_get_port_register $i 0x611) 0xff
			index_val=$(( 0x10 + $j ))
			regval=$(printf "0x%x" $index_val)
			
			ksz9567_spi_write $(ksz9567_get_port_register $i 0x60e) 0x0
			ksz9567_spi_write $(ksz9567_get_port_register $i 0x60f) 0x0 # dont map to any ruleset
			ksz9567_spi_write $(ksz9567_get_port_register $i 0x600) $regval # this entry, frn = current index
			ksz9567_spi_write $(ksz9567_get_port_register $i 0x612) $regval
		done
		

	done









        echo "***** EXPERIMENTING *****"

	# following https://microchip.my.site.com/s/article/KSZ9477-VLAN-Port-Forwarding-Setup
	echo "Setting up vlan 1 similar to microchip note"
	ksz9567_spi_write 0x310 0x65
	ksz9567_spi_write32 0x400 0x88000001
	ksz9567_spi_write32 0x404 0x3f
	ksz9567_spi_write32 0x408 0x3f
	ksz9567_spi_write16 0x40c 0x1
	ksz9567_spi_write 0x40e 0x81
	ksz9567_spi_write 0x312 0x04
	ksz9567_spi_write 0x310 0xe5


	for (( i=0; i<6; i++ )); do
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x1) 0x1
	done


	
	exit














	exit


	echo "Change Broadcast storm protection rate"
	ksz9567_spi_write 0x332 0x7
	ksz9567_spi_write 0x333 0xff
	
	echo "Setup priority 2 queue"
	ksz9567_spi_write32 0x390 0xc2

	exit



	exit

	echo "Change port priority control"
	for (( i=0; i<6; i++ )); do
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x801) 0x80
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x802) 0x80
		ksz9567_spi_write32 $(ksz9567_get_port_register $i 0x80c) 0x80
	
	done

	echo "Change port ACL mode"	
	for (( i=0; i<6; i++ )); do
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x803) 0x2
	done



	exit






	exit







	echo "Enable port vlan membership"
	# do for all ports, so 0-5
	for (( i=0; i<6; i++ )); do
		#enable port vlan membership
		ksz9567_spi_write32 $(ksz9567_get_port_register $i 0xa04) 0x3f
		ksz9567_spi_write $(ksz9567_get_port_register $i 0x1) 0x1
	done







	echo "Enabling unknown multicast forward to all ports"
	ksz9567_spi_write32 0x324 0x8000007F
	
	echo "Clean vlan table"
	ksz9567_clear_vlan_table
	
	#echo "Create default vlan"
	#ksz9567_write_vlan_table 1 1 1 0 0 0 0x3ff 0x3ff



	#echo "Enable VLAN mode??"
	ksz9567_spi_write 0x310 0xe1

	echo "Enable ACL forward no matter what??"
	for (( i=0; i<6; i++ )); do
		ksz9567_init_acl $i
	done


}






# ---- Helpers to extract bitfields from 32-bit values ----
# get_bits <value> <msb> <lsb>  → returns unsigned int
get_bits() {
    local v=$1 msb=$2 lsb=$3
    echo $(( ( (v >> lsb) & ((1 << (msb-lsb+1)) - 1) ) ))
}


# format 48-bit MAC from Entry3[15:0]=MAC[47:32], Entry4[31:0]=MAC[31:0]
format_mac48() {
    local e3=$1 e4=$2
    local mac_hi16=$(( e3 & 0xFFFF ))
    local mac_lo32=$(( e4 & 0xFFFFFFFF ))
    local mac48=$(( (mac_hi16 << 32) | mac_lo32 ))
    printf "%02x:%02x:%02x:%02x:%02x:%02x" \
        $(( (mac48 >> 40) & 0xFF )) \
        $(( (mac48 >> 32) & 0xFF )) \
        $(( (mac48 >> 24) & 0xFF )) \
        $(( (mac48 >> 16) & 0xFF )) \
        $(( (mac48 >>  8) & 0xFF )) \
        $(( (mac48 >>  0) & 0xFF ))
}


format_ports_bitmap() {
    local bm=$1
    local out=() p
    for ((p=0; p<7; p++)); do
        (( (bm >> p) & 1 )) && out+=("$((p+1))")
    done
    if ((${#out[@]}==0)); then
        printf "-"
    else
        (IFS=,; printf "%s" "${out[*]}")
    fi
}



decode_alu_entry() {
    # accept hex like 0x04000000
    local e1=$(( $1 ))
    local e2=$(( $2 ))
    local e3=$(( $3 ))
    local e4=$(( $4 ))

    # Entry1 fields
    local STATIC SRC_FILTER DES_FILTER PRI_AGE MSTP
    STATIC=$(( (e1 >> 31) & 1 ))
    SRC_FILTER=$(( (e1 >> 30) & 1 ))
    DES_FILTER=$(( (e1 >> 29) & 1 ))
    PRI_AGE=$(( (e1 >> 26) & 0x7 ))   # [28:26]
    MSTP=$((  e1        & 0x7 ))      # [2:0]

    # Entry2 fields
    local OVERRIDE PORT_FWD
    OVERRIDE=$(( (e2 >> 31) & 1 ))
    PORT_FWD=$((  e2        & 0x7F )) # [6:0]

    # Entry3/4 fields
    local FID MAC
    FID=$(( (e3 >> 16) & 0x7F ))      # [22:16]
    MAC=$(format_mac48 "$e3" "$e4")

    printf "  STATIC=%d, SRC_FILTER=%d, DES_FILTER=%d, PRI/AGE=%d, MSTP=%d\n" \
           "$STATIC" "$SRC_FILTER" "$DES_FILTER" "$PRI_AGE" "$MSTP"
    printf "  OVERRIDE=%d, PORT_FWD={%s}\n" \
           "$OVERRIDE" "$(format_ports_bitmap "$PORT_FWD")"
    printf "  FID=%d, MAC=%s\n" "$FID" "$MAC"
}




alu_debug() {
    # KSZ9567 ALU Table Access (search loop + decode)
    local REG_CTRL=0x418
    local REG_E1=0x420
    local REG_E2=0x424
    local REG_E3=0x428
    local REG_E4=0x42c

    local START_FINISH_BIT=7
    local VALID_BIT=6
    local VALID_COUNT_SHIFT=16
    local VALID_COUNT_MASK=$((0x3fff))

    local poll_attempts=20
    local poll_delay=0.005
    local max_entries=4096

    echo "ALU search: start"

    # ACTION=11 (search), START_FINISH=1 → 0x83
    ksz9567_spi_write32 "$REG_CTRL" 0x83

    local entries=0
    local reg valid e1 e2 e3 e4

    while (( entries < max_entries )); do
        # Poll VALID
        valid=0
        for ((i=0; i<poll_attempts; i++)); do
            reg=$(ksz9567_spi_read32 "$REG_CTRL")
            valid=$(( (reg >> VALID_BIT) & 1 ))
            (( valid )) && break
            sleep "$poll_delay"
        done
        if (( ! valid )); then
            printf "ALU search: VALID timeout. 0x418=0x%08x\n" "$((reg))"
            break
        fi

        # Read entries (keep original hex string for echo if you wish; we decode anyway)
        e1=$(ksz9567_spi_read32 "$REG_E1")
        e2=$(ksz9567_spi_read32 "$REG_E2")
        e3=$(ksz9567_spi_read32 "$REG_E3")
        e4=$(ksz9567_spi_read32 "$REG_E4")

        # Check all-zero => end of search
        if (( ((e1)) == 0 && ((e2)) == 0 && ((e3)) == 0 && ((e4)) == 0 )); then
            reg=$(ksz9567_spi_read32 "$REG_CTRL")
            local valid_count=$(( (reg >> VALID_COUNT_SHIFT) & VALID_COUNT_MASK ))
            printf "ALU search: done. VALID_COUNT=%d (0x%04x)\n" "$valid_count" "$valid_count"
            break
        fi

        echo "ALU entry[$entries]: $e1 $e2 $e3 $e4"
        decode_alu_entry "$e1" "$e2" "$e3" "$e4"
        ((entries++))

        # If START_FINISH cleared, we’re done; read VALID_COUNT and exit
        if (( ((reg >> START_FINISH_BIT) & 1) == 0 )); then
            reg=$(ksz9567_spi_read32 "$REG_CTRL")
            local valid_count=$(( (reg >> VALID_COUNT_SHIFT) & VALID_COUNT_MASK ))
            printf "ALU search: finished (START_FINISH=0). VALID_COUNT=%d (0x%04x)\n" \
                   "$valid_count" "$valid_count"
            break
        fi
        # Otherwise loop to poll the next valid entry
    done

    (( entries >= max_entries )) && echo "ALU search: hit safety max_entries=$max_entries"
}









handle_switch_command() {
    local cmd=$1
    shift

    if [ -z "$cmd" ]; then
        echo "Usage: $0 switch <init>"
        echo "  init        - Initialize the switch (global config, reset, etc)"
	echo "   alu_debug  - Print ALU information"
	echo "  synce-recover <1-5>   - Select front-panel port for SyncE recovery (placeholder)"
        return 1
    fi

    case "$cmd" in
        init) switch_init ;;
	alu_debug) alu_debug ;;
	synce-recover) synce_recover "$@" ;;
        *)
            echo "Unknown switch subcommand: $cmd"
            echo "Try: $0 switch"
            return 1
            ;;
    esac
}

