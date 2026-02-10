#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/../ksz9567_spi.sh"

ALL_PORTS=(1 2 3 4 5)

read_counters() {
    for phys_port in "$@"; do
        local log_port
        log_port=$(get_logical_port_or_error "$phys_port")
        echo "Reading counters for Port $phys_port (Logical $log_port)"
        for key in "${!port_counters[@]}"; do
            reg_addr="${port_counters[$key]}"
            reg_name="$key"
            val=$(ksz9567_get_port_counter $log_port $reg_addr)
            echo "Port $phys_port , $reg_name = $val"    
        done
    done
}

clear_counters() {
    for phys_port in "$@"; do
        local log_port
        log_port=$(get_logical_port_or_error "$phys_port")
        echo "Clearing counters for Port $phys_port (Logical $log_port)"
        # basically same as read, just ignore the results
        for key in "${!port_counters[@]}"; do
            reg_addr="${port_counters[$key]}"
            val=$(ksz9567_get_port_counter $log_port $reg_addr)
        done
    done
}

handle_counters_command() {
    local cmd=$1
    shift

    if [ -z "$cmd" ]; then
        echo "Usage: $0 counters <read|clear> [port_number...]"
        echo "  read [PORT]   - Read counters for one or more ports (or all if none given)"
        echo "  clear [PORT]  - Clear counters for one or more ports (or all if none given)"
        return 1
    fi

    local ports=("$@")
    if [ ${#ports[@]} -eq 0 ]; then
        ports=("${ALL_PORTS[@]}")
    fi

    case "$cmd" in
        read)  read_counters "${ports[@]}" ;;
        clear) clear_counters "${ports[@]}" ;;
        *)
            echo "Unknown counters subcommand: $cmd"
            echo "Try: $0 counters"
            return 1
            ;;
    esac
}

