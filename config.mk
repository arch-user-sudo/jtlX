
CFLAGS = -O2 -march=native -mtune=native -pipe \
         -fno-plt -fomit-frame-pointer \
         -fstack-protector-strong -D_FORTIFY_SOURCE=2 \
         -ffunction-sections -fdata-sections -flto

LDFLAGS = -Wl,--as-needed,--gc-sections -flto
_VERSION = jtl-v.1.4
VERSION  = `git describe --tags --dirty 2>/dev/null || echo $(_VERSION)`

PKG_CONFIG = pkg-config

# paths
PREFIX = /usr/local
MANDIR = $(PREFIX)/share/man
DATADIR = $(PREFIX)/share

# jtl.c is a self-contained amalgamation that inlines wlroots, so there is no
# wlroots library to link against.  These are kept empty for the build command.
WLR_INCS =
WLR_LIBS =

# Uncomment to build XWayland support
XWAYLAND = -DXWAYLAND
XLIBS = xcb xcb-icccm

# Uncomment to enable fullscreen tearing support (Mod+O enable, Mod+P disable)
FULLSCREEN_TEARING = -DFULLSCREEN_TEARING

# Uncomment to enable foreign toplevel management (for waybar, nwg-bar, etc.)
FOREIGN_TOPLEVEL = -DFOREIGN_TOPLEVEL

# Uncomment to enable the workspaces (ext-workspace-v1) protocol (for jtlab tray workspace popup)
WORKSPACES = -DWORKSPACES

# ---------------------------------------------------------------------------
# Wlroots feature toggles.  The flags above control #ifdefs in jtl.c; these
# control what goes INTO the amalgamated wlroots.c (which wlroots sources are
# pulled in and the matching WLR_HAS_* macros in the generated config.h), i.e.
# which wlroots features are compiled.  Toggling one re-runs amalgamate.py on
# the next make (config.mk is a prerequisite of wlroots.c).
# ---------------------------------------------------------------------------

# Uncomment to build the X11 backend into wlroots
# (all its deps - xcb, xcb-dri3, xcb-present, xcb-render, xcb-renderutil,
# xcb-shm, xcb-xfixes, xcb-xinput - are already in PKGS above).
# Note: named X11_BACKEND because `-DX11` would collide with jtl.c's
# `enum { XDGShell, LayerShell, X11 }`.
#X11_BACKEND = -DX11_BACKEND

# Uncomment to build the Vulkan renderer into wlroots.  Requires
# vulkan-loader and vulkan-headers, plus glslang (to compile the SPIR-V
# shaders while amalgamating).  Enable VULKAN_LIBS too so the loader links.
#VULKAN = -DVULKAN
#VULKAN_LIBS = vulkan

# dwl itself only uses C99 features, but wlroots' headers use anonymous unions (C11).
# To avoid warnings about them, we do not use -std=c99 and instead of using the
# gmake default 'CC=c99', we use cc.
CC = cc
