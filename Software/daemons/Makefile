# Makefile for installing + controlling Switchberry daemon services, scripts, and configs
#
# This Makefile is intended to:
#  - install systemd units into /usr/lib/systemd/system (or $(SYSTEMD_DIR))
#  - install helper scripts into /usr/local/sbin (or $(SBIN_DIR))
#  - install configs into /etc/switchberry (or $(CONF_DIR))
#  - provide "make start/stop/restart/status/logs" targets for the full stack
#
# Notes:
#  - Some services are optional depending on your image (e.g. gpsd.service).
#  - ptp4l/ts2phc services are typically started by switchberry-ptp-role.service,
#    but we include them in stop/status helpers so you can debug easily.

PREFIX        ?= /usr
SYSTEMD_DIR   ?= $(PREFIX)/lib/systemd/system
SBIN_DIR      ?= /usr/local/sbin
CONF_DIR      ?= /etc/switchberry
GPSD_DEFAULT_FILE ?= /etc/default/gpsd

INSTALL ?= install

# ---------------------------
# Files in this repo directory
# ---------------------------

# Systemd service units expected to live alongside this Makefile
SERVICE_FILES := \
    switchberry-apply-timing.service \
    switchberry-full-init.service \
    switchberry-dpll-monitor.service \
    switchberry-ptp-role.service \
    ts2phc-switchberry.service \
    ptp4l-switchberry-gm.service \
    ptp4l-switchberry-client.service \
    switchberry-cm4-pps-monitor.service \
    gpspipe-socat.service

# Helper scripts in this directory that should go into /usr/local/sbin
SCRIPT_FILES := \
    ptp4l_dpll_quality_guard.sh \
    ts2phc_dpll_guard.sh \
    ptp4l_gm_guard.sh \
    switchberry_cm4_pps_monitor.sh \
    switchberry-ptp-role.sh

# Timing/PTP config files that should go into /etc/switchberry
CONF_FILES := \
    ts2phc-switchberry.conf \
    ptp4l-switchberry-gm-uc.conf \
    ptp4l-switchberry-client-uc.conf

# GPSD default config template in this directory (optional)
GPSD_CONF_SRC := switchberry-gpsd.conf

# Only install files that exist (so the Makefile can be used across partial checkouts)
SERVICE_FILES_EXISTING := $(wildcard $(SERVICE_FILES))
SCRIPT_FILES_EXISTING  := $(wildcard $(SCRIPT_FILES))
CONF_FILES_EXISTING    := $(wildcard $(CONF_FILES))

SERVICE_FILES_MISSING := $(filter-out $(SERVICE_FILES_EXISTING),$(SERVICE_FILES))
SCRIPT_FILES_MISSING  := $(filter-out $(SCRIPT_FILES_EXISTING),$(SCRIPT_FILES))
CONF_FILES_MISSING    := $(filter-out $(CONF_FILES_EXISTING),$(CONF_FILES))

# ---------------------------
# Service groups (systemctl)
# ---------------------------

# Core boot chain (required)
SERVICES_CORE := \
    switchberry-apply-timing.service \
    switchberry-full-init.service \
    switchberry-dpll-monitor.service

# GNSS plumbing (optional; gpsd.service is usually provided by the OS image)
SERVICES_GNSS := \
    gpsd.service \
    gpspipe-socat.service

# Role orchestration (required)
SERVICES_ROLE_ORCH := \
    switchberry-ptp-role.service

# Role-specific daemons (usually started by the role orchestrator)
SERVICES_PTP := \
    ts2phc-switchberry.service \
    ptp4l-switchberry-gm.service \
    ptp4l-switchberry-client.service \
    switchberry-cm4-pps-monitor.service

# Everything we might want to stop/status (safe even if not running)
SERVICES_ALL := $(SERVICES_PTP) $(SERVICES_ROLE_ORCH) $(SERVICES_GNSS) $(SERVICES_CORE)

# Start/stop order (dependency aware)
START_ORDER := $(SERVICES_CORE) $(SERVICES_GNSS) $(SERVICES_ROLE_ORCH)
STOP_ORDER  := $(SERVICES_PTP) $(SERVICES_ROLE_ORCH) $(SERVICES_GNSS) $(SERVICES_CORE)

# Enable/disable on boot (recommended defaults)
# We enable the core chain + role orchestrator.
# We do NOT enable ptp4l/ts2phc directly (role script decides).
ENABLE_DEFAULT := $(SERVICES_CORE) $(SERVICES_ROLE_ORCH)
# Optional: include GNSS by default if you want:
# ENABLE_DEFAULT := $(SERVICES_CORE) $(SERVICES_GNSS) $(SERVICES_ROLE_ORCH)

# ---------------------------
# Targets
# ---------------------------

.PHONY: all help \
        install uninstall \
        install-services install-scripts install-configs install-gpsd-config \
        daemon-reload enable disable \
        start stop restart status logs \
        start-core stop-core restart-core \
        start-ptp stop-ptp restart-ptp \
        start-gnss stop-gnss restart-gnss

all: help

help:
	@echo "Switchberry Makefile targets:"
	@echo ""
	@echo "  make install           Install units/scripts/configs + daemon-reload + enable"
	@echo "  make uninstall         Remove units/scripts/configs installed by this Makefile"
	@echo ""
	@echo "  make enable            Enable default boot services: $(ENABLE_DEFAULT)"
	@echo "  make disable           Disable default boot services"
	@echo "  make daemon-reload     systemctl daemon-reload"
	@echo ""
	@echo "  make start             Start full stack in order: $(START_ORDER)"
	@echo "  make stop              Stop full stack in order:  $(STOP_ORDER)"
	@echo "  make restart           Stop then start (ordered)"
	@echo "  make status            Show status for all relevant services"
	@echo "  make logs S=<unit>     Follow logs for a unit (example: make logs S=switchberry-full-init.service)"
	@echo ""
	@echo "  make start-core|stop-core|restart-core   Manage core chain"
	@echo "  make start-gnss|stop-gnss|restart-gnss   Manage GNSS plumbing"
	@echo "  make start-ptp|stop-ptp|restart-ptp      Manage PTP daemons (role-specific)"
	@echo ""
	@echo "Notes:"
	@echo "  - switchberry-ptp-role.service typically starts/stops ptp4l/ts2phc based on config."
	@echo "  - gpsd.service is often provided by the OS image; failures to start it may be normal."

# ---------------------------
# Install
# ---------------------------

install: install-services install-scripts install-configs install-gpsd-config daemon-reload #enable
	@echo "Install complete."

install-services:
	@echo "Installing systemd units into $(DESTDIR)$(SYSTEMD_DIR)"
	$(INSTALL) -d "$(DESTDIR)$(SYSTEMD_DIR)"
	@if [ -n "$(SERVICE_FILES_MISSING)" ]; then \
		echo "WARNING: Missing service files (not installed): $(SERVICE_FILES_MISSING)"; \
	fi
	@for f in $(SERVICE_FILES_EXISTING); do \
		echo "  installing $$f -> $(DESTDIR)$(SYSTEMD_DIR)/$$f"; \
		$(INSTALL) -m 0644 "$$f" "$(DESTDIR)$(SYSTEMD_DIR)/$$f"; \
	done

install-scripts:
	@echo "Installing helper scripts into $(DESTDIR)$(SBIN_DIR)"
	$(INSTALL) -d "$(DESTDIR)$(SBIN_DIR)"
	@if [ -n "$(SCRIPT_FILES_MISSING)" ]; then \
		echo "WARNING: Missing script files (not installed): $(SCRIPT_FILES_MISSING)"; \
	fi
	@for f in $(SCRIPT_FILES_EXISTING); do \
		echo "  installing $$f -> $(DESTDIR)$(SBIN_DIR)/$$f"; \
		$(INSTALL) -m 0755 "$$f" "$(DESTDIR)$(SBIN_DIR)/$$f"; \
	done

install-configs:
	@echo "Installing Switchberry config files into $(DESTDIR)$(CONF_DIR)"
	$(INSTALL) -d "$(DESTDIR)$(CONF_DIR)"
	@if [ -n "$(CONF_FILES_MISSING)" ]; then \
		echo "WARNING: Missing config files (not installed): $(CONF_FILES_MISSING)"; \
	fi
	@for f in $(CONF_FILES_EXISTING); do \
		echo "  installing $$f -> $(DESTDIR)$(CONF_DIR)/$$f"; \
		$(INSTALL) -m 0644 "$$f" "$(DESTDIR)$(CONF_DIR)/$$f"; \
	done

install-gpsd-config:
	@echo "Installing GPSD default config..."
	@if [ -f "$(GPSD_CONF_SRC)" ]; then \
		echo "  source: $(GPSD_CONF_SRC) -> $(DESTDIR)$(GPSD_DEFAULT_FILE)"; \
		$(INSTALL) -d "$(DESTDIR)$(dir $(GPSD_DEFAULT_FILE))"; \
		$(INSTALL) -m 0644 "$(GPSD_CONF_SRC)" "$(DESTDIR)$(GPSD_DEFAULT_FILE)"; \
	else \
		echo "  WARNING: GPSD config source '$(GPSD_CONF_SRC)' not found, skipping."; \
	fi

uninstall:
	@echo "Removing systemd units from $(SYSTEMD_DIR)"
	@for f in $(SERVICE_FILES); do \
		echo "  removing $(SYSTEMD_DIR)/$$f"; \
		rm -f "$(SYSTEMD_DIR)/$$f"; \
	done
	@echo "Removing helper scripts from $(SBIN_DIR)"
	@for f in $(SCRIPT_FILES); do \
		echo "  removing $(SBIN_DIR)/$$f"; \
		rm -f "$(SBIN_DIR)/$$f"; \
	done
	@echo "Removing config files from $(CONF_DIR)"
	@for f in $(CONF_FILES); do \
		echo "  removing $(CONF_DIR)/$$f"; \
		rm -f "$(CONF_DIR)/$$f"; \
	done
	@echo "Removing GPSD default config from $(GPSD_DEFAULT_FILE)"
	rm -f "$(GPSD_DEFAULT_FILE)"

# ---------------------------
# Systemd helpers
# ---------------------------

daemon-reload:
	@echo "Reloading systemd daemon"
	sudo systemctl daemon-reload

enable:
	@echo "Enabling default Switchberry boot services:"
	@for s in $(ENABLE_DEFAULT); do \
		echo "  enable $$s"; \
		sudo systemctl enable "$$s"; \
	done

disable:
	@echo "Disabling default Switchberry boot services:"
	@for s in $(ENABLE_DEFAULT); do \
		echo "  disable $$s"; \
		sudo systemctl disable "$$s" || true; \
	done

# ---------------------------
# Stack control (ordered)
# ---------------------------

start:
	@echo "Starting Switchberry stack (ordered):"
	@for s in $(START_ORDER); do \
		echo "  start $$s"; \
		sudo systemctl start "$$s" || exit $$?; \
	done

stop:
	@echo "Stopping Switchberry stack (ordered):"
	@for s in $(STOP_ORDER); do \
		echo "  stop $$s"; \
		sudo systemctl stop "$$s" || true; \
	done

restart: stop start
	@true

status:
	@echo "Status: Switchberry services"
	@sudo systemctl --no-pager status $(SERVICES_ALL) || true

logs:
	@if [ -z "$(S)" ]; then \
		echo "Usage: make logs S=<unit.service>"; \
		echo "Example: make logs S=switchberry-full-init.service"; \
		exit 1; \
	fi
	sudo journalctl -u "$(S)" -f

# ---------------------------
# Group controls
# ---------------------------

start-core:
	@for s in $(SERVICES_CORE); do sudo systemctl start "$$s" || exit $$?; done
stop-core:
	@for s in $(SERVICES_CORE); do sudo systemctl stop "$$s" || true; done
restart-core: stop-core start-core

start-gnss:
	@for s in $(SERVICES_GNSS); do sudo systemctl start "$$s" || true; done
stop-gnss:
	@for s in $(SERVICES_GNSS); do sudo systemctl stop "$$s" || true; done
restart-gnss: stop-gnss start-gnss

start-ptp:
	@for s in $(SERVICES_PTP); do sudo systemctl start "$$s" || true; done
stop-ptp:
	@for s in $(SERVICES_PTP); do sudo systemctl stop "$$s" || true; done
restart-ptp: stop-ptp start-ptp

