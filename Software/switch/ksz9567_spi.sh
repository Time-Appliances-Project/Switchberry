#!/bin/bash
# Set these as needed:
SPI_DEV="/dev/spidev0.0"
SPI_SPEED=1000000     # Adjust to match your hardware




declare -A port_counters
port_counters["RxByteCnt"]=0x80
port_counters["RxDropPackets"]=0x82
port_counters["RxHiPriorityBytes"]=0x0
port_counters["RxUndersizePkt"]=0x1
port_counters["RxFragments"]=0x2
port_counters["RxOversize"]=0x3
port_counters["RxJabbers"]=0x4
port_counters["RxSymbolError"]=0x5
port_counters["RxCRCerror"]=0x6
port_counters["RxAlignmentError"]=0x7
port_counters["RxControl8808Pkts"]=0x8
port_counters["RxPausePkts"]=0x9
port_counters["RxBroadcast"]=0xa
port_counters["RxMulticast"]=0xb
port_counters["RxUnicast"]=0xc
#port_counters["Rx64Octets"]=0xd
#port_counters["Rx65to127Octets"]=0xe
#port_counters["Rx128to255Octets"]=0xf
#port_counters["Rx256to511Octets"]=0x10
#port_counters["Rx512to2023Octets"]=0x11
#port_counters["Rx1024to1522Octets"]=0x12
#port_counters["Rx1523to2000Octets"]=0x13
#port_counters["Rx2001+Octets"]=0x14
port_counters["TxHiPriorityBytes"]=0x15
port_counters["TxLateCollision"]=0x16
port_counters["TxPausePkts"]=0x17
port_counters["TxBroadcastPkts"]=0x18
port_counters["TxMulticastPkts"]=0x19
port_counters["TxUnicastPkts"]=0x1a
port_counters["TxDeferred"]=0x1b
port_counters["TxTotalCollision"]=0x1c
port_counters["TxExcessiveCollision"]=0x1d
port_counters["TxSingleCollision"]=0x1e
port_counters["TxMultipleCollision"]=0x1f
port_counters["TxByteCnt"]=0x81
port_counters["TxDropPackets"]=0x83





# Switchberry front-panel (physical) <-> KSZ logical port mapping
# Physical = left-to-right as seen on the front panel: 1 2 3 4 5
# Logical  = KSZ register port numbering
#
# physical -> logical
#   1 -> 5
#   2 -> 1
#   3 -> 2
#   4 -> 3
#   5 -> 4

is_valid_physical_port() { [[ "$1" =~ ^[1-5]$ ]]; }
is_valid_logical_port()  { [[ "$1" =~ ^[1-5]$ ]]; }

physical_to_logical() {
  local p="$1"
  if ! is_valid_physical_port "$p"; then
    echo "ERROR: physical port must be 1-5, got '$p'" >&2
    return 1
  fi
  case "$p" in
    1) echo 5 ;;
    2) echo 1 ;;
    3) echo 2 ;;
    4) echo 3 ;;
    5) echo 4 ;;
  esac
}

logical_to_physical() {
  local l="$1"
  if ! is_valid_logical_port "$l"; then
    echo "ERROR: logical port must be 1-5, got '$l'" >&2
    return 1
  fi
  case "$l" in
    5) echo 1 ;;
    1) echo 2 ;;
    2) echo 3 ;;
    3) echo 4 ;;
    4) echo 5 ;;
  esac
}

# Helper: pretty-print the mapping (handy for --help output)
print_port_map() {
  cat <<'EOF'
Switchberry port map (physical -> logical):
  1 -> 5
  2 -> 1
  3 -> 2
  4 -> 3
  5 -> 4
EOF
}

# Example usage:
#   lp="$(physical_to_logical 1)"  # => 5
#   pp="$(logical_to_physical 5)"  # => 1






ksz9567_spi_read() {
    local addr="$1"

    #echo "Got arg $1"

    local cmd=$(printf '\\x%02x' $(( 0x60 | ((addr >> 19) & 0x1F) )) )
    local b1=$(printf '\\x%02x' $(( (addr >> 11) & 0xFF )) )
    local b2=$(printf '\\x%02x' $(( (addr >> 3) & 0xFF )) )
    local b3=$(printf '\\x%02x' $(( (addr & 0x7) << 5 )) )
    local d0="\\x00"

    #echo "DEBUG: addr=$addr cmd=$cmd b1=$b1 b2=$b2 b3=$b3"

    local pattern
    pattern=$(printf "%s%s%s%s%s" $cmd $b1 $b2 $b3 $d0)
    #echo "DEBUG: pattern: $pattern"

    local out
    #spidev_test -D $SPI_DEV -b 8 -s $SPI_SPEED -p "$pattern" -v
    out=$(spidev_test -D $SPI_DEV -b 8 -s $SPI_SPEED -p "$pattern" -v 2>/dev/null | grep -A1 "RX |" | tail -n1 | cut -d" " -f 7)
    #echo "Got 0x$out"
    #printf "Read KSZ9567 reg 0x%06x = 0x%s\n" "$addr" "$out"
    echo "0x$out"
}


ksz9567_spi_write() {
    # Usage: ksz9567_spi_write <register_addr_hex> <value_hex>
    local addr="$1"

    #echo "Got arg $1"

    local cmd=$(printf '\\x%02x' $(( 0x40 | ((addr >> 19) & 0x1F) )) )
    local b1=$(printf '\\x%02x' $(( (addr >> 11) & 0xFF )) )
    local b2=$(printf '\\x%02x' $(( (addr >> 3) & 0xFF )) )
    local b3=$(printf '\\x%02x' $(( (addr & 0x7) << 5 )) )
    local d0=$(printf '\\x%02x' $(( $2 & 0xff )) )

    #echo "DEBUG: addr=$addr cmd=$cmd b1=$b1 b2=$b2 b3=$b3 d0=$d0"

    local pattern
    pattern=$(printf "%s%s%s%s%s" $cmd $b1 $b2 $b3 $d0)
    #echo "DEBUG: pattern: $pattern"

    local out
    ignore=$(spidev_test -D $SPI_DEV -b 8 -s $SPI_SPEED -p "$pattern" -v 2>/dev/null)
}



ksz9567_spi_read16() {
    # Usage: ksz9567_spi_read16 <reg_addr>
    local addr=$(( $1 ))
    local msb=$(ksz9567_spi_read $addr | awk '{print $NF}' | sed 's/0x//')
    local lsb=$(ksz9567_spi_read $((addr+1)) | awk '{print $NF}' | sed 's/0x//')
    printf "0x%04x\n" $(( (0x$msb << 8) | 0x$lsb ))
}

ksz9567_spi_write16() {
    # Usage: ksz9567_spi_write16 <reg_addr> <16bit_value>
    local addr=$(( $1 ))
    local value=$(( $2 ))

    local cmd=$(printf '\\x%02x' $(( 0x40 | ((addr >> 19) & 0x1F) )) )
    local b1=$(printf '\\x%02x' $(( (addr >> 11) & 0xFF )) )
    local b2=$(printf '\\x%02x' $(( (addr >> 3) & 0xFF )) )
    local b3=$(printf '\\x%02x' $(( (addr & 0x7) << 5 )) )

    # 16-bit value is big endian: MSB first
    local d0=$(printf '\\x%02x' $(( (value >> 8) & 0xFF )) )
    local d1=$(printf '\\x%02x' $(( value & 0xFF )) )

    local pattern
    pattern=$(printf "%s%s%s%s%s%s" $cmd $b1 $b2 $b3 $d0 $d1)

    ignore=$(spidev_test -D $SPI_DEV -b 8 -s $SPI_SPEED -p "$pattern" -v 2>/dev/null)
}

ksz9567_spi_read32() {
    # Usage: ksz9567_spi_read32 <reg_addr>
    local addr=$(( $1 ))
    local b3=$(ksz9567_spi_read $addr | awk '{print $NF}' | sed 's/0x//')
    local b2=$(ksz9567_spi_read $((addr+1)) | awk '{print $NF}' | sed 's/0x//')
    local b1=$(ksz9567_spi_read $((addr+2)) | awk '{print $NF}' | sed 's/0x//')
    local b0=$(ksz9567_spi_read $((addr+3)) | awk '{print $NF}' | sed 's/0x//')
    printf "0x%08x\n" $(( (0x$b3 << 24) | (0x$b2 << 16) | (0x$b1 << 8) | 0x$b0 ))
}

ksz9567_spi_write32() {
    # Usage: ksz9567_spi_write32 <reg_addr> <32bit_value>
    local addr=$(( $1 ))
    local value=$(( $2 ))

    # SPI write command with address
    local cmd=$(printf '\\x%02x' $(( 0x40 | ((addr >> 19) & 0x1F) )) )
    local b1=$(printf '\\x%02x' $(( (addr >> 11) & 0xFF )) )
    local b2=$(printf '\\x%02x' $(( (addr >> 3) & 0xFF )) )
    local b3=$(printf '\\x%02x' $(( (addr & 0x7) << 5 )) )

    # 32-bit data, big endian (MSB first)
    local d0=$(printf '\\x%02x' $(( (value >> 24) & 0xFF )) )
    local d1=$(printf '\\x%02x' $(( (value >> 16) & 0xFF )) )
    local d2=$(printf '\\x%02x' $(( (value >> 8) & 0xFF )) )
    local d3=$(printf '\\x%02x' $(( value & 0xFF )) )

    local pattern
    pattern=$(printf "%s%s%s%s%s%s%s%s" $cmd $b1 $b2 $b3 $d0 $d1 $d2 $d3)

    ignore=$(spidev_test -D $SPI_DEV -b 8 -s $SPI_SPEED -p "$pattern" -v 2>/dev/null)
}


ksz9567_phy_mmd_write() {
	local port=$(( $1 ))
	local mmd=$(( $2 ))
	local register=$(( $3 ))
	local value=$(( $4 ))

	local setup_reg_addr=$(ksz9567_get_port_register $port 0x11a)
	local data_reg_addr=$(ksz9567_get_port_register $port 0x11c)

	setup1=$(printf "0x%x" $(( $mmd )) )
	data1=$(printf "0x%x" $(( $register )) )
	setup2=$(printf "0x%x" $(( 0x4000 + $mmd )))
	data2=$(printf "0x%x" $value)

	ksz9567_spi_write16 $setup_reg_addr $setup1
	ksz9567_spi_write16 $data_reg_addr $data1
	ksz9567_spi_write16 $setup_reg_addr $setup2
	ksz9567_spi_write16 $data_reg_addr $data2
}

ksz9567_get_port_register() {
	local addr=$(( ( ($1 + 1) << 12) + $2))
	echo $(printf "0x%x" $addr)
}

ksz9567_get_port_counter() {
	# $1 is port number, 0 based
	# $2 is counter address, page 202 of datasheet
	# accessed via indirect registers page 166
	csr_addr=$(ksz9567_get_port_register $1 0x500)
	data_addr=$(ksz9567_get_port_register $1 0x504)

	index_val=$( printf '0x%x' $(( ( ($2 & 0xff) << 16 ) )) )
	addr_val=$( printf '0x%x' $(( ( ($2 & 0xff) << 16 ) + (1<<25) )) )
	#echo "Write $csr_addr = $addr_val"

	ksz9567_spi_write32 $csr_addr $index_val

	ksz9567_spi_write32 $csr_addr $addr_val


	lower_val=$(ksz9567_spi_read32 $data_addr)
	upper_val=$(ksz9567_spi_read32 $csr_addr)
	#echo "Data_addr = $data_addr, val=$lower_val"
	#echo "Csr_addr = $csr_addr, val=$upper_val"
	val=$(( ($lower_val & 0xffffffff) + ( ($upper_val & 0xf) << 32 ) ))
	#echo $(printf "0x%x" $val)
	echo $(printf "%d" $val)
}

ksz9567_read_port_link_status() {
	# $1 is port number, 0 based
	val=$(ksz9567_spi_read $(ksz9567_get_port_register $1 0x13))
	if (( (val & 0x2) == 0x2 )); then
		echo "Up"
	else
		echo "Down"
	fi
}

ksz9567_read_port_status() {
	# $1 is port number, 0 based
	val=$(ksz9567_spi_read $(ksz9567_get_port_register $1 0x30))
	speed=$(( ($val >> 3) & 0x3 ))
	if (( speed == 0 )); then
		echo "10Mb/s"
	elif (( speed == 1 )); then
		echo "100Mb/s"
	elif (( speed == 2 )); then
		echo "1Gb/s"
	else
		echo "Unknown"
	fi
}


ksz9567_read_vlan_table() {
	# usage: ksz9567_read_vlan_table 2
	# only argument is the vlan ID
	# dumps in human readable format
	# use registers listed on page 199 of datasheet to read vlan table, definitions on page 108
	val=$(printf "0x%x" $(( ( $1 & 0xfff ) )) )
	ksz9567_spi_write16 0x40c $val # index register
	ksz9567_spi_write 0x40e 0x82 # trigger register, do read
	val0=$(ksz9567_spi_read32 0x400)
	val1=$(ksz9567_spi_read32 0x404)
	val2=$(ksz9567_spi_read32 0x408)

	# parse the three vlan registers
	echo "Vlan table, 0x400=$(printf '0x%x' $val0) , 0x404=$(printf '0x%x' $val1) , 0x408=$(printf '0x%x' $val2)"
	
}
ksz9567_write_vlan_table() {
	# argument list:
	# $1 = vlan number, 0-4095
	# $2 = valid , set it to valid or not
	# $3 = 1 for forward based on port forward bitmask, 0 for other
	# $4 = priority, 3-bit priority
	# $5 = MSTP (Multiple spanning tree protocol index) , 3-bits, idk, set to zero
	# $6 = Filter ID, idk set to zero
	# $7 = port untag, bitmask, 1 to untag packets upon egress, idk set to 0x3ff 
	# $8 = port forward, bitmask, 1 to forward to this port, 0 to not forward

	# basically use registers listed on page 199 of datasheet to setup and write to vlan table

	val0=$(( ( ($2 & 0x1) << 31) ))
	val0=$(( $val0 + ( ( $3 & 0x1 ) << 27 ) ))
	val0=$(( $val0 + ( ( $4 & 0x7 ) << 24 ) ))
	val0=$(( $val0 + ( ( $5 & 0x7 ) << 12 ) ))
	val0=$(( $val0 + ( ( $6 & 0x3f ) << 0 ) ))

	val1=$(( $7 & 0x3ff ))

	val2=$(( $8 & 0x3ff ))

	val3=$(( $1 & 0xfff ))

	ksz9567_spi_write32 0x400 $(printf "0x%x" $val0)
	ksz9567_spi_write32 0x404 $(printf "0x%x" $val1)
	ksz9567_spi_write32 0x408 $(printf "0x%x" $val2)
	ksz9567_spi_write32 0x40c $(printf "0x%x" $val3)

	ksz9567_spi_write 0x40e 0x81 # trigger register, do write
}

ksz9567_clear_vlan_table() {
	ksz9567_spi_write 0x40e 0x83 # trigger register, do clear
}

ksz9567_init_acl() {
	# $1 is port number 

	# ksz9477_acl_port_enable
	val=$(ksz9567_spi_read $(ksz9567_get_port_register $1 0x801) )
	val=$(( $val | 0x1 ))
	val=$(( $val | 0x40 ))
	ignore=$(ksz9567_spi_write $(ksz9567_get_port_register $1 0x801) $val)




	# trigger read of all ACL registers for acl rule 0
	ignore=$(ksz9567_spi_write $(ksz9567_get_port_register $1 0x610) 0xff)
	ignore=$(ksz9567_spi_write $(ksz9567_get_port_register $1 0x611) 0xff)
	ignore=$(ksz9567_spi_write $(ksz9567_get_port_register $1 0x612) 0x0)
	# setup forward to all ports no matter what I think 
	ignore=$(ksz9567_spi_write $(ksz9567_get_port_register $1 0x601) 0x0)
	ignore=$(ksz9567_spi_write $(ksz9567_get_port_register $1 0x60d) 0x7f)
	# trigger write back of ACL index 0
	ignore=$(ksz9567_spi_write $(ksz9567_get_port_register $1 0x612) 0x10)
}



# Example usage:
# ksz9567_spi_write 0x123 0x55
# ksz9567_spi_read 0x123
#echo "Got 0x1 = $(ksz9567_spi_read 0x1)" # id register check
#echo "Got 0x103 = $(ksz9567_spi_read 0x103)"


#echo "16-bit register 0x30a = $(ksz9567_spi_read16 0x30a)"
#echo "32-bit register 0x414 = $(ksz9567_spi_read32 0x414)"
#echo "Write 32-bit register 0x414 = 0xdeadbeef $(ksz9567_spi_write32 0x414 0xdeadbeef)"
#echo "32-bit register 0x414 = $(ksz9567_spi_read32 0x414)"


#######
# Enable all ports 

# phy basic control register, set autoneg and normal operation
# only needed for the copper base-t ports, so 0-4
#for (( i=0; i<5; i++ )); do
#	ksz9567_spi_write16 $(ksz9567_get_port_register $i 0x100) 0x1300
#done

# small sleep, let autoneg go through
#sleep 2


# that's all you need to do to enable basic L1 link, now need to enable packet forwarding

# do for all ports, so 0-5
#for (( i=0; i<6; i++ )); do
#	#enable port vlan membership
#	ksz9567_spi_write32 $(ksz9567_get_port_register $i 0xa04) 0x7f
#done

# global setting, should be there be re-enable, enable unicast learning control
#ksz9567_spi_write 0x0311 0xc5


#######
# Print port status and counters
#for (( i=0; i<6; i++ )); do
#    echo "********* Port $i ***********"
#    echo "Port $i, Link $(ksz9567_read_port_link_status $i) , Speed $(ksz9567_read_port_status $i)"
#    echo ""
#
#done

#for (( i=0; i<6; i++ )); do
#    echo "********* Port $i ***********"
#	for key in "${!port_counters[@]}"; do
#	    reg_addr="${port_counters[$key]}"
#	    reg_name="$key"
#	    val=$(ksz9567_get_port_counter $i $reg_addr)
#	    echo "$reg_name = $val"
#	done
#    echo ""
#done

