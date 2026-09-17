
CFLAGS = -O2 -march=native -mtune=native -pipe -flto \
         -fomit-frame-pointer \
         -fstack-protector-strong -D_FORTIFY_SOURCE=2 -DNDEBUG \
         -ffunction-sections -fdata-sections

LDFLAGS = -flto -Wl,--as-needed,--gc-sections
_VERSION = jtl-v.1.4
VERSION  = `git describe --tags --dirty 2>/dev/null || echo $(_VERSION)`

PKG_CONFIG = pkg-config

# paths
PREFIX = /usr/local
MANDIR = $(PREFIX)/share/man
DATADIR = $(PREFIX)/share

# Uncomment to build XWayland support
XWAYLAND = -DXWAYLAND
XLIBS = xcb xcb-composite xcb-ewmh xcb-icccm xcb-render xcb-res xcb-xfixes xcb-shape

# Uncomment to enable fullscreen tearing support (Mod+O enable, Mod+P disable)
FULLSCREEN_TEARING = -DFULLSCREEN_TEARING

# Uncomment to enable foreign toplevel management (for waybar, nwg-bar, etc.)
FOREIGN_TOPLEVEL = -DFOREIGN_TOPLEVEL

# Uncomment to enable the workspaces (ext-workspace-v1) protocol (for jtlab tray workspace popup)
WORKSPACES = -DWORKSPACES

# dwl itself only uses C99 features, but wlroots' headers use anonymous unions (C11).
# To avoid warnings about them, we do not use -std=c99 and instead of using the
# gmake default 'CC=c99', we use cc.
CC = cc
