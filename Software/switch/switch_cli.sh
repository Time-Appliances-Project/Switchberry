#!/bin/bash

case "$1" in
    port)
        shift
        source ./commands/port.sh
        handle_port_command "$@"
        ;;
    counters)
        shift
        source ./commands/counters.sh
        handle_counters_command "$@"
        ;;
    switch)
        shift
        source ./commands/switch.sh
        handle_switch_command "$@"
        ;;
    *)
        echo "Usage: $0 {port|counters|switch} [args...]"
        exit 1
        ;;
esac

