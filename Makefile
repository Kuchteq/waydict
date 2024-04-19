# Optimize, turn on additional warnings
CFLAGS ?= -std=c17 -O3
PKG_CONFIG ?= pkg-config

WAYLAND_FLAGS = $(shell $(PKG_CONFIG) wayland-client --cflags --libs)
WAYLAND_PROTOCOLS_DIR = $(shell $(PKG_CONFIG) wayland-protocols --variable=pkgdatadir)

# Build deps
WAYLAND_SCANNER = $(shell pkg-config --variable=wayland_scanner wayland-scanner)

XDG_SHELL_PROTOCOL = $(WAYLAND_PROTOCOLS_DIR)/stable/xdg-shell/xdg-shell.xml

HEADERS=state.h draw.h xdg-shell-client-protocol.h shm.h keyboard.h draw_help.h config.h fetching.h json/json.h
SOURCES=main.c xdg-shell-protocol.c shm.c draw.c keyboard.c draw_help.c fetching.c json/json.c
.PHONY: all
all: elf
CFLAGS+=$(shell pkg-config --cflags wayland-client wayland-cursor fcft pixman-1 xkbcommon libcurl)
LDLIBS+=$(shell pkg-config --libs wayland-client wayland-cursor fcft pixman-1 xkbcommon libcurl) -lrt -lm

elf: $(HEADERS) $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(WAYLAND_FLAGS)  $(LDLIBS)

xdg-shell-client-protocol.h:
	$(WAYLAND_SCANNER) client-header $(XDG_SHELL_PROTOCOL) xdg-shell-client-protocol.h

xdg-shell-protocol.c:
	$(WAYLAND_SCANNER) private-code $(XDG_SHELL_PROTOCOL) xdg-shell-protocol.c

.PHONY: clean
clean:
	rm -f elf
