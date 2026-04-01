#!/bin/bash


SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/../ksz9567_spi.sh"


ALL_PORTS=(1 2 3 4 5)

port_enable() {
    for phys_port in "$@"; do
        local log_port
        log_port=$(get_logical_port_or_error "$phys_port")
        echo "Enabling Port $phys_port (Logical $log_port)"
        ksz9567_spi_write16 $(ksz9567_get_port_register $log_port 0x100) 0x1300
    done
}

port_disable() {
    for phys_port in "$@"; do
        local log_port
        log_port=$(get_logical_port_or_error "$phys_port")
        echo "Disabling Port $phys_port (Logical $log_port)"
        ksz9567_spi_write16 $(ksz9567_get_port_register $log_port 0x100) 0xc00
    done
}

port_status() {
    for phys_port in "$@"; do
        local log_port
        log_port=$(get_logical_port_or_error "$phys_port")
        status=$(ksz9567_read_port_link_status "$log_port")
        echo "Port $phys_port : $status"
    done
}


handle_port_command() {
    local cmd=$1
    shift

    if [ -z "$cmd" ]; then
        echo "Usage: $0 port <enable|disable|status> [port_number...]"
        echo "  enable [PORT]     - Enable one or more ports (or all if none given)"
        echo "  disable [PORT]    - Disable one or more ports (or all if none given)"
        echo "  status [PORT]     - Show status for one or more ports (or all if none given)"
        return 1
    fi

    local ports=("$@")
    if [ ${#ports[@]} -eq 0 ]; then
        ports=("${ALL_PORTS[@]}")
    fi

    case "$cmd" in
        enable)  port_enable "${ports[@]}" ;;
        disable) port_disable "${ports[@]}" ;;
        status)  port_status "${ports[@]}" ;;
        *)
            echo "Unknown port subcommand: $cmd"
            echo "Try: $0 port"
            return 1
            ;;
    esac
}

