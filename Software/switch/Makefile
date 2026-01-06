# Makefile for installing Switchberry KSZ switch init bundle and service

PREFIX             ?= /usr
INSTALL_ROOT       ?= /usr/local/lib/switchberry
INSTALL_DIR        ?= $(INSTALL_ROOT)/kszswitch-init
SBIN_DIR           ?= /usr/local/sbin

INSTALL            ?= install

# All files in this directory that should be copied to the central location.
INIT_FILES := \
    full_init.sh \
    phy_init.sh \
    ksz9567_spi.sh \
    switch_cli.sh

# Subdirectories that should be copied recursively (e.g. 'commands/')
INIT_DIRS := \
    commands

.PHONY: all install install-files install-links uninstall daemon-reload enable disable

all:
	@echo "Nothing to build here. Use 'make install' to install init bundle "

# Single entry point: copy files, create PATH links
install: install-files install-links daemon-reload enable
	@echo "Install complete."

install-files: $(INIT_FILES)
	@echo "Installing init bundle into $(DESTDIR)$(INSTALL_DIR)"
	$(INSTALL) -d "$(DESTDIR)$(INSTALL_DIR)"

	@for f in $(INIT_FILES); do \
		mode=0644; \
		case "$$f" in \
			*.sh) mode=0755 ;; \
		esac; \
		echo "  installing $$f (mode=$$mode)"; \
		$(INSTALL) -m $$mode "$$f" "$(DESTDIR)$(INSTALL_DIR)/$$f"; \
	done

	@for d in $(INIT_DIRS); do \
		if [ -d "$$d" ]; then \
			echo "  copying directory $$d -> $(DESTDIR)$(INSTALL_DIR)/$$d"; \
			rm -rf "$(DESTDIR)$(INSTALL_DIR)/$$d"; \
			cp -a "$$d" "$(DESTDIR)$(INSTALL_DIR)/"; \
		else \
			echo "  WARNING: directory $$d not found, skipping"; \
		fi; \
	done

install-links:
	@echo "Creating convenience symlinks in $(DESTDIR)$(SBIN_DIR)"
	$(INSTALL) -d "$(DESTDIR)$(SBIN_DIR)"
	# full_init.sh entry point
	ln -sf "$(INSTALL_DIR)/full_init.sh" "$(DESTDIR)$(SBIN_DIR)/full_init.sh"
	# switch_cli.sh entry point
	ln -sf "$(INSTALL_DIR)/switch_cli.sh" "$(DESTDIR)$(SBIN_DIR)/switch_cli.sh"

uninstall:
	@echo "Removing init bundle from $(INSTALL_DIR)"
	rm -rf "$(INSTALL_DIR)"

	@echo "Removing symlinks from $(SBIN_DIR)"
	rm -f "$(SBIN_DIR)/full_init.sh"
	rm -f "$(SBIN_DIR)/switch_cli.sh"


