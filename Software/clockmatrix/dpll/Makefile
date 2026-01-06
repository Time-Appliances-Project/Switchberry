# Makefile for 8A3400x DPLL Linux SPI utility + TCS parser + table API

# Compiler and flags
CC      := gcc
CFLAGS  := -O2 -g -Wall -Wextra

# Libraries (uncomment -lrt if your platform needs it for clock_gettime)
LDLIBS  := -lm
# LDLIBS += -lrt

# ---- dplltool (unchanged) ----
SRCS    := dpll_utility.c linux_dpll.c tcs_dpll.c renesas_cm8a34001_tables.c cm_dpll_utils.c
OBJS    := $(SRCS:.c=.o)
TARGET  := dplltool


# Install locations
PREFIX  ?= /usr/local
SBINDIR ?= $(PREFIX)/sbin
FIX_SCRIPT := fastlock_1pps_fix.sh

.PHONY: all clean install

all: $(TARGET) 

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)


# Generic rule: each .o depends on its .c and the common headers
%.o: %.c linux_dpll.h tcs_dpll.h renesas_cm8a34001_tables.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJS) $(TARGET) 

install: $(TARGET) $(DISC_TARGET)
	install -d $(DESTDIR)$(SBINDIR)
	install -m 0755 $(TARGET) $(DESTDIR)$(SBINDIR)/$(TARGET)
	install -m 0755 $(FIX_SCRIPT) $(DESTDIR)$(SBINDIR)/$(FIX_SCRIPT)

