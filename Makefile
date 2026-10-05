.POSIX:
.SUFFIXES:

include config.mk

# flags for compiling
DWLCPPFLAGS = -I. -DWLR_USE_UNSTABLE -D_POSIX_C_SOURCE=200809L \
	-DVERSION=\"$(VERSION)\" $(XWAYLAND) $(FULLSCREEN_TEARING) $(FOREIGN_TOPLEVEL) $(WORKSPACES)
DWLDEVCFLAGS = -g -Wpedantic -Wall -Wextra -Wdeclaration-after-statement \
	-Wno-unused-parameter -Wshadow -Wunused-macros -Werror=strict-prototypes \
	-Werror=implicit -Werror=return-type -Werror=incompatible-pointer-types \
	-Wfloat-conversion -Wimplicit-fallthrough

# CFLAGS / LDFLAGS.  jtl.c is a self-contained amalgamation: wlroots is inlined
# rather than linked, so the link line only needs wlroots' own system deps.
PKGS      = wayland-server xkbcommon libinput pixman-1 libdrm egl gbm glesv2 \
	lcms2 libudev libseat libdisplay-info libliftoff wayland-client \
	xcb xcb-dri3 xcb-present xcb-render xcb-renderutil xcb-shm xcb-xfixes \
	xcb-xinput xcb-composite xcb-ewmh xcb-icccm xcb-res xcb-shape $(XLIBS)
DWLCFLAGS = `$(PKG_CONFIG) --cflags $(PKGS)` $(WLR_INCS) $(DWLCPPFLAGS) $(DWLDEVCFLAGS) $(CFLAGS)
LDLIBS    = `$(PKG_CONFIG) --libs $(PKGS)` $(WLR_LIBS) -lm $(LIBS)

# The generated source and the tooling that produces it.  The hand-written
# compositor lives in jtl.c.orig; `make amalgamate` rewrites jtl.c from it.
AMALG_DIR = tools/amalgamate
AMALG     = $(AMALG_DIR)/amalgamate.py
SCAN      = $(AMALG_DIR)/scan.py

all: jtl
jtl: jtl.o
	$(CC) jtl.o $(DWLCFLAGS) $(LDFLAGS) $(LDLIBS) -o $@
jtl.o: jtl.c config.h config.mk

# Regenerate the self-contained source when the hand-written source changes.
jtl.c: jtl.c.orig $(AMALG) $(SCAN)
	python3 $(AMALG)

# Force a regeneration even if jtl.c looks up to date.
amalgamate:
	python3 $(AMALG)

config.h:
	cp config.def.h $@

clean:
	rm -f jtl *.o

dist: clean
	mkdir -p jtl-$(VERSION)
	cp -R LICENSE* Makefile CHANGELOG.md README.md config.def.h \
		config.mk protocols jtl.c jtl.c.orig tools jtl.desktop \
		jtl-$(VERSION)
	tar -caf jtl-$(VERSION).tar.gz jtl-$(VERSION)
	rm -rf jtl-$(VERSION)

install: jtl
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	rm -f $(DESTDIR)$(PREFIX)/bin/jtl
	cp -f jtl $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/jtl
	mkdir -p $(DESTDIR)$(DATADIR)/wayland-sessions
	cp -f jtl.desktop $(DESTDIR)$(DATADIR)/wayland-sessions/jtl.desktop
	chmod 644 $(DESTDIR)$(DATADIR)/wayland-sessions/jtl.desktop
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/jtl \
		$(DESTDIR)$(DATADIR)/wayland-sessions/jtl.desktop

.PHONY: all clean amalgamate install uninstall dist

.SUFFIXES: .c .o
.c.o:
	$(CC) $(CPPFLAGS) $(DWLCFLAGS) -o $@ -c $<
