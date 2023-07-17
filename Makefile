WAYLAND_SCANNER = $(shell pkg-config --variable=wayland_scanner wayland-scanner)
WAYLAND_PROTOCOLS_DIR = $(shell pkg-config wayland-protocols --variable=pkgdatadir)
XDG_SHELL_PROTOCOL = $(WAYLAND_PROTOCOLS_DIR)/stable/xdg-shell/xdg-shell.xml
SOURCES=hello.c shm.c image.c xdg-shell-protocol.c

all: helloworld
	echo "done"

clean:
	rm helloworld

helloworld: $(SOURCES) xdg-shell-client-protocol.h
	gcc -o $@ $(SOURCES) -lwayland-client -lcairo -lfreeimage

xdg-shell-client-protocol.h:
	$(WAYLAND_SCANNER) client-header $(XDG_SHELL_PROTOCOL) $@

xdg-shell-protocol.c:
	$(WAYLAND_SCANNER) private-code $(XDG_SHELL_PROTOCOL) $@




