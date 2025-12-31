# Makefile for switchberry/clockmatrix Python tools

# Where to install by default
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/sbin

INSTALL ?= install

# Python scripts in this directory
PY_SCRIPTS := apply_timing.py config.py

# Names to install them as (drop .py)
INSTALL_NAMES := apply_timing clockmatrix-config

.PHONY: all install uninstall clean

all:
	@echo "Nothing to build in clockmatrix; use 'make install' to install scripts."

install: $(PY_SCRIPTS)
	@echo "Installing ClockMatrix Python tools into $(DESTDIR)$(BINDIR)"
	$(INSTALL) -d "$(DESTDIR)$(BINDIR)"
	# apply_timing.py -> apply_timing
	$(INSTALL) -m 0755 apply_timing.py "$(DESTDIR)$(BINDIR)/apply_timing"
	# config.py as importable module
	$(INSTALL) -m 0644 config.py "$(DESTDIR)$(BINDIR)/config.py"
	# optional convenience wrapper: config.py -> clockmatrix-config
	$(INSTALL) -m 0755 config.py "$(DESTDIR)$(BINDIR)/clockmatrix-config"

uninstall:
	@echo "Removing ClockMatrix Python tools from $(DESTDIR)$(BINDIR)"
	rm -f "$(DESTDIR)$(BINDIR)/apply_timing"
	rm -f "$(DESTDIR)$(BINDIR)/clockmatrix-config"

clean:
	@echo "Nothing to clean in clockmatrix."

