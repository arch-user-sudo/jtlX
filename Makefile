include config.mk

# build directory
BUILD = build

# wayland-scanner generates C bindings for Wayland protocols
WAYLAND_SCANNER   = `$(PKG_CONFIG) --variable=wayland_scanner wayland-scanner`
WAYLAND_PROTOCOLS = `$(PKG_CONFIG) --variable=pkgdatadir wayland-protocols`

# wlroots source list — canonical list vendored beside the Makefile.
# Regenerate after updating wlroots with:
#   cd wlroots && find . -name '*.c' \
#     -not -path './subprojects/*' -not -path './tinywl/*' \
#     -not -path './examples/*' -not -path './test/*' \
#     -not -path './docs/*' | sed 's|^\./||' \
#     | grep -vE '^backend/x11/(backend|output|input_device)\.c$$' \
#     | grep -vE '^render/vulkan/(renderer|texture|pixel_format|vulkan|util|pass)\.c$$' \
#     | grep -vE 'libliftoff\.c$$|color_lcms2\.c$$|dmabuf_fallback\.c$$' | sort
WLR_SRC := $(shell cat wlroots.build-files.txt)
XWAYLAND_SRC = xwayland/server.c xwayland/shell.c xwayland/sockets.c \
	xwayland/xwayland.c xwayland/xwm.c \
	xwayland/selection/selection.c xwayland/selection/dnd.c \
	xwayland/selection/incoming.c xwayland/selection/outgoing.c
# Drop wlroots' XWayland module (and its xcb link deps) when XWAYLAND is off
ifdef XWAYLAND
NO_XW_OBJ =
else
NO_XW_OBJ = $(XWAYLAND_SRC:%.c=$(BUILD)/%.o)
endif
WLR_OBJ = $(filter-out $(NO_XW_OBJ),$(WLR_SRC:%.c=$(BUILD)/%.o))
WLR_HAS_XWAYLAND = $(if $(XWAYLAND),1,0)

# protocol basenames — mirrors wlroots/protocol/meson.build
PROTO_NAMES = \
	linux-dmabuf-v1 \
	presentation-time \
	tablet-v2 \
	viewporter \
	xdg-shell \
	alpha-modifier-v1 \
	color-management-v1 \
	color-representation-v1 \
	content-type-v1 \
	cursor-shape-v1 \
	drm-lease-v1 \
	ext-background-effect-v1 \
	ext-foreign-toplevel-list-v1 \
	ext-idle-notify-v1 \
	ext-image-capture-source-v1 \
	ext-image-copy-capture-v1 \
	ext-session-lock-v1 \
	ext-data-control-v1 \
	ext-workspace-v1 \
	fractional-scale-v1 \
	linux-drm-syncobj-v1 \
	pointer-warp-v1 \
	security-context-v1 \
	single-pixel-buffer-v1 \
	xdg-activation-v1 \
	xdg-dialog-v1 \
	xdg-system-bell-v1 \
	xdg-toplevel-icon-v1 \
	xdg-toplevel-tag-v1 \
	xwayland-shell-v1 \
	tearing-control-v1 \
	idle-inhibit-unstable-v1 \
	keyboard-shortcuts-inhibit-unstable-v1 \
	pointer-constraints-unstable-v1 \
	pointer-gestures-unstable-v1 \
	primary-selection-unstable-v1 \
	relative-pointer-unstable-v1 \
	text-input-unstable-v3 \
	xdg-decoration-unstable-v1 \
	xdg-foreign-unstable-v1 \
	xdg-foreign-unstable-v2 \
	xdg-output-unstable-v1 \
	ext-transient-seat-v1 \
	drm \
	input-method-unstable-v2 \
	server-decoration \
	virtual-keyboard-unstable-v1 \
	wlr-data-control-unstable-v1 \
	wlr-export-dmabuf-unstable-v1 \
	wlr-foreign-toplevel-management-unstable-v1 \
	wlr-gamma-control-unstable-v1 \
	wlr-layer-shell-unstable-v1 \
	wlr-output-management-unstable-v1 \
	wlr-output-power-management-unstable-v1 \
	wlr-screencopy-unstable-v1 \
	wlr-virtual-pointer-unstable-v1

PROTO_OBJ = $(addprefix $(BUILD)/protocol/,$(addsuffix -protocol.o,$(PROTO_NAMES)))
PNPID_OBJ = $(BUILD)/backend/drm/pnpids.o
LIFTOFF_SRC = alloc.c device.c layer.c list.c log.c output.c plane.c
LIFTOFF_OBJ = $(addprefix $(BUILD)/libliftoff/,$(LIFTOFF_SRC:%.c=%.o))
DI_SRC = cta.c cta-vic.c cta-vic-table.c cvt.c displayid.c displayid2.c dmt.c \
	dmt-table.c edid.c gtf.c hdmi-vic.c info.c log.c memory-stream.c
DI_OBJ = $(addprefix $(BUILD)/libdisplay-info/,$(DI_SRC:%.c=%.o)) \
	$(BUILD)/libdisplay-info/pnp-id-table.o
PIXMAN_SRC = pixman.c pixman-access.c pixman-access-accessors.c pixman-arm.c \
	pixman-bits-image.c pixman-combine32.c pixman-combine-float.c \
	pixman-conical-gradient.c pixman-edge.c pixman-edge-accessors.c \
	pixman-fast-path.c pixman-filter.c pixman-glyph.c pixman-general.c \
	pixman-gradient-walker.c pixman-image.c pixman-implementation.c \
	pixman-linear-gradient.c pixman-matrix.c pixman-mips.c pixman-noop.c \
	pixman-ppc.c pixman-radial-gradient.c pixman-region16.c \
	pixman-region32.c pixman-region64f.c pixman-riscv.c pixman-solid-fill.c \
	pixman-timer.c pixman-trap.c pixman-utils.c pixman-x86.c
PIXMAN_COM_OBJ = $(addprefix $(BUILD)/pixman/,$(PIXMAN_SRC:%.c=%.o))
PIXMAN_SIMD_OBJ = $(addprefix $(BUILD)/pixman/,pixman-mmx.o pixman-sse2.o pixman-ssse3.o)
PIXMAN_CONFIG = $(BUILD)/pixman/pixman-config.h $(BUILD)/pixman/pixman-version.h
# libwayland source list — mirrors libwayland/src/meson.build. libwayland-util
# plus the private connection/OS helpers, libwayland-server and
# libwayland-client (wlroots' wayland backend needs the client side).
# libwayland-cursor and libwayland-egl are client-side helpers, not needed here.
# The generated core protocol marshalling code is shared by the server and the
# client library, so it is compiled exactly once.
WAYLAND_SRC = wayland-util.c connection.c wayland-os.c \
	wayland-server.c wayland-shm.c event-loop.c wayland-client.c
WAYLAND_OBJ = $(addprefix $(BUILD)/libwayland/,$(WAYLAND_SRC:%.c=%.o)) \
	$(BUILD)/wayland/wayland-protocol.o
CONFIG_HEADERS = $(BUILD)/include/config.h $(BUILD)/include/wlr/config.h $(BUILD)/include/wlr/version.h
ALL_OBJS = $(WLR_OBJ) $(PROTO_OBJ) $(PNPID_OBJ) $(LIFTOFF_OBJ) $(DI_OBJ) \
	$(PIXMAN_COM_OBJ) $(PIXMAN_SIMD_OBJ) $(WAYLAND_OBJ) $(LIBINPUT_OBJ) \
	$(XKBCOMMON_OBJ) $(XKBCOMMON_PARSER_OBJ)

# ---- Mesa (vendored, RadeonSI only) ----
#
# Mesa is deliberately *not* built with LTO. Upstream refuses it ("Building
# Mesa with LTO is not supported"), and trying it anyway was a bad trade here:
# the jtl link took over 12 minutes and had not finished, and the partially
# written binary was already 57 MB against 28 MB for the plain link. The rest of
# jtl keeps its -flto. These flags have to be written as a JSON array: meson
# does not split the comma form into separate arguments, and repeated -Dopt+=
# does not accumulate.
#
# Mesa is built by Meson as a set of static libraries into $(MESA_PREFIX) and
# linked into jtl like every other vendored library here. Two Mesa patches make
# that possible: libgbm and its DRI backend honour default_library, and
# -Dstatic-gbm-backend links the backend into libgbm instead of having libgbm
# dlopen() dri_gbm.so (the only dlopen() left in this graphics stack - wlroots
# resolves EGL/GLES through eglGetProcAddress, and Mesa's own EGL/GLESv2 link
# Gallium directly). Everything below stays a normal jtl link input, so there is
# no rpath, no backend search path and no system Mesa involved.
MESA_DIR = mesa
MESA_BUILD = $(BUILD)/mesa
MESA_PREFIX = $(abspath $(MESA_BUILD)/prefix)
MESA_LIBDIR = $(MESA_PREFIX)/lib
# where `make install` puts the Mesa tree, so an installed jtl keeps working

# Bare minimum for this machine: RadeonSI on the amdgpu kernel driver. No
# Vulkan (wlroots' Vulkan renderer is disabled as well), no GLX, no desktop
# OpenGL, no X11/Wayland window-system platform code (the compositor only needs
# the GBM and surfaceless EGL platforms) and no tests.
MESA_OPTS = \
	--prefix=$(MESA_PREFIX) --libdir=lib \
	-Dbuild-tests=false \
	-Dgallium-drivers=radeonsi \
	-Dvulkan-drivers= \
	-Dglvnd=disabled \
	-Dopengl=false -Dglx=disabled \
	-Dgles1=disabled -Dgles2=enabled -Degl=enabled -Dgbm=enabled \
	-Dplatforms= \
	-Dllvm=enabled \
	-Dvalgrind=disabled -Dlibunwind=disabled -Dstrip=true \
	-Doptimization=2 -Ddebug=false \
	"-Dc_args=['-march=native','-mtune=native','-pipe']" \
	"-Dcpp_args=['-march=native','-mtune=native','-pipe']" \
	-Dspirv-tools=disabled \
	-Db_ndebug=true \
	--default-library=static \
	-Dstatic-gbm-backend=true \
	-Dgallium-va=disabled

# Meson's static libraries hold only their own objects (the link_with/link_whole
# dependencies stay separate), so the whole archive set is linked. Order is
# irrelevant inside the group. The system libraries are what the shared Mesa
# objects used to need.
MESA_LIBS = -Wl,--start-group $$(find $(MESA_BUILD) -name '*.a') -Wl,--end-group \
	-L/usr/lib/llvm/22/lib64 -lLLVM-22 \
	-ldrm -ldrm_amdgpu -lexpat -lz -lzstd -lelf -lstdc++

# PKGS used by both compilation and linking (wlroots, libliftoff, libdisplay-info,
# pixman, libwayland and Mesa are vendored, so their own .pc files are no longer
# needed; libwayland still needs libffi, and so do we now)
# libinput 1.32 parses evdev through libevdev (1.31.3 did its own), so the
# vendored copy needs its headers and its library.
PKGS = libevdev libdrm libffi libseat libudev $(XLIBS)
# Mesa is vendored as well, so only its headers come from the system
PKGS_HEADERS = egl glesv2 gbm
# EGL/GLES/gbm headers come from the vendored Mesa so that they match the
# headers the archives in MESA_LIBS were built against.
PKG_CFLAGS = -I$(MESA_DIR)/include `$(PKG_CONFIG) --cflags $(PKGS)` `$(PKG_CONFIG) --cflags $(PKGS_HEADERS)`
PKG_LIBS = `$(PKG_CONFIG) --libs $(PKGS)`

# libwayland compile flags. libwayland's private sources include "../config.h",
# so the generated config header is put one directory above a quoted-include
# directory to make that resolve to $(BUILD)/wayland/config.h
WAYLAND_DEFS = -D_POSIX_C_SOURCE=200809L
WAYLAND_INCS = -iquote $(BUILD)/wayland/include -I$(BUILD)/wayland \
	-Ilibwayland/src
WAYLAND_CFLAGS = $(WAYLAND_DEFS) $(WAYLAND_INCS) \
	`$(PKG_CONFIG) --cflags libffi` $(CFLAGS)

# wlroots compile flags
WLRDEFS = -D_POSIX_C_SOURCE=200809L -DWLR_USE_UNSTABLE -DWLR_PRIVATE= \
	-DWLR_LITTLE_ENDIAN=1 -DWLR_BIG_ENDIAN=0
WLRWARN = -Wundef -Wlogical-op -Wmissing-include-dirs -Wold-style-definition \
	-Wpointer-arith -Winit-self -Wstrict-prototypes \
	-Wimplicit-fallthrough=2 -Wendif-labels -Wstrict-aliasing=2 \
	-Woverflow -Wmissing-prototypes -Walloca \
	-Wno-missing-braces -Wno-missing-field-initializers -Wno-unused-parameter
WLRINCS = -I$(BUILD)/include -Iwlroots/include -I$(BUILD)/protocol -I$(BUILD)/shaders \
	-I$(BUILD)/wayland -Ilibwayland/src \
	-Ilibliftoff/include -Ilibdisplay-info/include -Ipixman -I$(BUILD)/pixman \
	-I$(LIBINPUT_PUBLIC_INC) -I$(XKBCOMMON_PUBLIC_INC) \
	$(PKG_CFLAGS)
WLR_CFLAGS = $(WLRDEFS) $(WLRWARN) $(WLRINCS) $(CFLAGS)

# libinput source list — canonical list vendored beside the Makefile, same
# shape as wlroots.build-files.txt. Regenerate after updating libinput with:
#   cd libinput && ls src/*.c \
#     | grep -vE 'libinput-plugin-(lua|mtdev)\.c$$' | sort \
#     > ../libinput.build-files.txt
# The two plugins are left out because this build has neither Lua nor mtdev -
# upstream only adds them when dep_lua.found()/have_mtdev hold, and
# libinput-plugin-mtdev.c includes <mtdev-plumbing.h> unconditionally, so it
# would not even compile without those headers. The system libinput 1.31.3 this
# replaces was built without mtdev and libwacom too, so libwacom stays off as
# well and udev remains the only new-ish dependency.
LIBINPUT_SRC := $(shell cat libinput.build-files.txt)
LIBINPUT_OBJ = $(LIBINPUT_SRC:%.c=$(BUILD)/libinput/%.o)
LIBINPUT_VERSION := $(shell sed -n "s/.*version *: *'\([^']*\)'.*/\1/p" libinput/meson.build | head -1)
LIBINPUT_VER_MAJOR := $(word 1,$(subst ., ,$(LIBINPUT_VERSION)))
LIBINPUT_VER_MINOR := $(word 2,$(subst ., ,$(LIBINPUT_VERSION)))
LIBINPUT_VER_MICRO := $(word 3,$(subst ., ,$(LIBINPUT_VERSION)))

# libinput's private sources include "config.h" and "libinput-version.h",
# which meson generates upstream. Both are reproduced under $(BUILD)/libinput
# so the quoted includes resolve there instead of picking up jtl's own
# config.h out of $(BUILD)/include (-iquote only applies to "..." includes).
LIBINPUT_GEN = $(BUILD)/libinput/config.h \
	$(BUILD)/libinput/include/libinput-version.h \
	$(BUILD)/libinput/include/libinput.h
# Public include dir, laid out like an installed libinput: libinput.h plus
# libinput-version.h and nothing else. wlroots and jtl see only this, so
# <linux/input.h> keeps resolving to the kernel header (as it did against the
# system libinput, which installs its headers flat) rather than to libinput's
# bundled copy.
LIBINPUT_PUBLIC_INC = $(BUILD)/libinput/include
# -Ilibinput is not optional: a few sources include "src/evdev-frame.h", i.e.
# paths relative to the libinput root, which is what meson's
# include_directories('.') provides upstream.
LIBINPUT_INCS = -iquote $(BUILD)/libinput -I$(LIBINPUT_PUBLIC_INC) \
	-Ilibinput -Ilibinput/src -Ilibinput/include
# libinput-util.h warns if NDEBUG is set: libinput leans on assert() for its
# invariants, so it is dropped here rather than for the whole build.
LIBINPUT_CFLAGS = $(LIBINPUT_INCS) $(PKG_CFLAGS) $(filter-out -DNDEBUG,$(CFLAGS))

# Vendored xkbcommon 1.13.2, the same version Gentoo has installed, so the
# behaviour jtl saw from the system library is unchanged. The tree holds only
# what the library is built from: src/, include/, meson.build, meson_options.txt
# and LICENSE. test/, tools/, doc/, bench/, fuzz/, scripts/, changes/, .github/
# and every dotfile were never downloaded. This is the "same as wlroots"
# treatment: sources compiled straight into jtl, no libxkbcommon.so.
#
# src/x11/ is deliberately absent — jtl is Wayland-only and the X11 backend is
# disabled in the wlroots config, so libxkbcommon-x11 would be dead weight.

XKBCOMMON_SRC := $(shell cat xkbcommon.build-files.txt)
XKBCOMMON_OBJ = $(XKBCOMMON_SRC:%.c=$(BUILD)/xkbcommon/%.o)
XKBCOMMON_VERSION := $(shell sed -n "s/.*version *: *'\([^']*\)'.*/\1/p" xkbcommon/meson.build | head -1)

# xkbcommon source list — taken from upstream's own list rather than globbed,
# so a file meson does not compile cannot sneak in, and the generated parser is
# left out (it has its own rule below). Regenerate after updating xkbcommon:
#   sed -n "/^libxkbcommon_sources = \[/,/^\]/p" xkbcommon/meson.build \
#     | grep -oE "'src/[^']+\.c'" | tr -d "'" | sort > xkbcommon.build-files.txt

# Sources include "config.h", which meson writes from an inline
# configuration_data() block (there is no config.h.in in the tree), plus the
# bison-generated parser.h, so both live under $(BUILD)/xkbcommon. The parser
# is generated rather than compiled from the list, so it gets its own object.
XKBCOMMON_PARSER = $(BUILD)/xkbcommon/src/xkbcomp/parser.c
XKBCOMMON_PARSER_H = $(BUILD)/xkbcommon/src/xkbcomp/parser.h
XKBCOMMON_PARSER_OBJ = $(BUILD)/xkbcommon/src/xkbcomp/parser.o
XKBCOMMON_PUBLIC_HEADERS = \
	$(BUILD)/xkbcommon/include/xkbcommon/xkbcommon.h \
	$(BUILD)/xkbcommon/include/xkbcommon/xkbcommon-keysyms.h \
	$(BUILD)/xkbcommon/include/xkbcommon/xkbcommon-compose.h \
	$(BUILD)/xkbcommon/include/xkbcommon/xkbcommon-compat.h \
	$(BUILD)/xkbcommon/include/xkbcommon/xkbcommon-names.h \
	$(BUILD)/xkbcommon/include/xkbcommon/xkbregistry.h
XKBCOMMON_GEN = $(BUILD)/xkbcommon/config.h \
	$(XKBCOMMON_PARSER) $(XKBCOMMON_PARSER_H) $(XKBCOMMON_PUBLIC_HEADERS)

# Laid out like an installed xkbcommon: just include/xkbcommon/*.h. The x11
# header is left out to match the missing src/x11/. wlroots and jtl include
# <xkbcommon/xkbcommon.h>, so this is all they ever see.
XKBCOMMON_PUBLIC_INC = $(BUILD)/xkbcommon/include

# -Ixkbcommon is not optional: a few sources include "src/utils.h" and
# "src/messages-codes.h", i.e. paths relative to the xkbcommon root, which is
# what meson's include_directories('.') gives upstream. The generated parser.h
# is found through its own directory. No -Ixkbcommon/include: consumers get the
# public dir instead, so a source cannot reach past the installed layout.
XKBCOMMON_INCS = -I$(BUILD)/xkbcommon -I$(XKBCOMMON_PUBLIC_INC) \
	-Ixkbcommon -Ixkbcommon/src -I$(BUILD)/xkbcommon/src/xkbcomp

# xkbcommon asserts on its own invariants in keymap.c, state.c, text.c,
# utils.h and utils-checked-arithmetic.h, so as with libinput, -DNDEBUG is
# dropped for these sources only.
XKBCOMMON_CFLAGS = $(XKBCOMMON_INCS) $(PKG_CFLAGS) $(filter-out -DNDEBUG,$(CFLAGS))

# jtl.o compile flags
DWLCPPFLAGS = -I. -DWLR_USE_UNSTABLE -D_POSIX_C_SOURCE=200809L \
	-DVERSION=\"$(VERSION)\" $(XWAYLAND) $(FULLSCREEN_TEARING) \
	$(FOREIGN_TOPLEVEL) $(WORKSPACES) \
	-DWLR_PRIVATE= -DWLR_LITTLE_ENDIAN=1 -DWLR_BIG_ENDIAN=0
DWLDEVCFLAGS = -Wpedantic -Wall -Wextra -Wdeclaration-after-statement \
	-Wno-unused-parameter -Wshadow -Wunused-macros -Werror=strict-prototypes \
	-Werror=implicit -Werror=return-type -Werror=incompatible-pointer-types \
	-Wfloat-conversion -Wimplicit-fallthrough
DWLCFLAGS = -Ipixman -I$(BUILD)/pixman \
	$(PKG_CFLAGS) -I$(BUILD)/include -Iwlroots/include \
	-I$(BUILD)/protocol -I$(BUILD)/wayland -Ilibwayland/src \
	-I$(LIBINPUT_PUBLIC_INC) -I$(XKBCOMMON_PUBLIC_INC) \
	$(DWLCPPFLAGS) $(DWLDEVCFLAGS) $(CFLAGS)

LDLIBS = $(PKG_LIBS) $(MESA_LIBS) -lm -lpthread

all: jtl

# Mesa is an order-only prerequisite: it has to be there before we link, but
# rebuilding it should not force jtl to be relinked. Makefile and config.mk are
# normal prerequisites so that changing a compile or link flag does relink, with
# the objects listed explicitly so they don't end up on the link line.
jtl: jtl.o $(ALL_OBJS) Makefile config.mk | mesa
	$(CC) jtl.o $(ALL_OBJS) $(LDFLAGS) $(LDLIBS) -o $@

# ---- Mesa sub-build ----

# meson/ninja are incremental, so `meson compile` is cheap when Mesa is already
# up to date and picks up edits to the vendored sources without a reconfigure.
ifneq ($(filter clean,$(MAKECMDGOALS)),)
else
mesa: $(MESA_BUILD)/build.ninja
	meson compile -C $(MESA_BUILD)
	DESTDIR= meson install -C $(MESA_BUILD) --only-changed
endif

# meson regenerates build.ninja itself when meson.build/meson.options change,
# this only covers a fresh (or wiped) build directory
$(MESA_BUILD)/build.ninja: mesa/meson.build mesa/meson.options mesa/VERSION Makefile
	@mkdir -p $(MESA_BUILD)
	@if [ -f $@ ]; then \
		meson setup --reconfigure $(MESA_BUILD) $(MESA_DIR) $(MESA_OPTS); \
	else \
		meson setup $(MESA_BUILD) $(MESA_DIR) $(MESA_OPTS); \
	fi

# ---- generated config headers ----

$(BUILD)/include/config.h:
	@mkdir -p $(BUILD)/include
	@printf '%s\n' \
	'/* Autogenerated by the Meson build system. */' \
	'#pragma once' '' \
	'#define HAVE_XCB_ERRORS 0' '' \
	'#define HAVE_EGL 1' '' \
	'#define HAVE_LIBLIFTOFF 1' '' \
	'#define HAVE_LIBLIFTOFF_0_5 1' '' \
	'#define HAVE_EVENTFD 1' '' \
	'#define HAVE_LINUX_SYNC_FILE 1' '' \
	'#define HAVE_LIBINPUT_BUSTYPE 1' '' \
	'#define HAVE_LIBINPUT_SWITCH_KEYPAD_SLIDE 1' '' \
	'#define ICONDIR "/usr/share/icons"' > $@
	@if [ -n "$(XWAYLAND)" ]; then printf '%s\n' \
	'#define HAVE_XWAYLAND_LISTENFD 1' '' \
	'#define HAVE_XWAYLAND_NO_TOUCH_POINTER_EMULATION 1' '' \
	'#define HAVE_XWAYLAND_FORCE_XRANDR_EMULATION 1' '' \
	'#define HAVE_XWAYLAND_TERMINATE_DELAY 1' '' \
	'#define XWAYLAND_PATH "/usr/bin/Xwayland"' >> $@; fi

$(BUILD)/include/wlr/config.h:
	@mkdir -p $(BUILD)/include/wlr
	@printf '%s\n' \
	'#ifndef WLR_CONFIG_H' \
	'#define WLR_CONFIG_H' '' \
	'#define WLR_HAS_DRM_BACKEND 1' \
	'#define WLR_HAS_LIBINPUT_BACKEND 1' \
	'#define WLR_HAS_X11_BACKEND 0' \
	'#define WLR_HAS_GLES2_RENDERER 1' \
	'#define WLR_HAS_VULKAN_RENDERER 0' \
	'#define WLR_HAS_GBM_ALLOCATOR 1' \
	'#define WLR_HAS_UDMABUF_ALLOCATOR 1' \
	'#define WLR_HAS_XWAYLAND $(WLR_HAS_XWAYLAND)' \
	'#define WLR_HAS_SESSION 1' \
	'#define WLR_HAS_COLOR_MANAGEMENT 0' '' \
	'#endif' > $@

$(BUILD)/include/wlr/version.h:
	@mkdir -p $(BUILD)/include/wlr
	@printf '%s\n' \
	'#ifndef WLR_VERSION_H' \
	'#define WLR_VERSION_H' '' \
	'#define WLR_VERSION_STR "0.21.0-dev"' '' \
	'#define WLR_VERSION_MAJOR 0' \
	'#define WLR_VERSION_MINOR 21' \
	'#define WLR_VERSION_MICRO 0' '' \
	'#define WLR_VERSION_NUM ((WLR_VERSION_MAJOR << 16) | (WLR_VERSION_MINOR << 8) | WLR_VERSION_MICRO)' '' \
	'int wlr_version_get_major(void);' \
	'int wlr_version_get_minor(void);' \
	'int wlr_version_get_micro(void);' '' \
	'#endif' > $@

# ---- libwayland generated headers and core protocol ----

# wayland.xml is scanned with the system wayland-scanner, exactly like the
# protocol XMLs from wayland-protocols below. The -c variants only swap the
# wayland-{server,client}.h include for the matching -core.h one. config.h
# mirrors the probes from libwayland/meson.build on Linux/glibc, so note the
# header probes are #undef and not #define 0 — libwayland tests them with
# #ifdef (sys/ucred.h is gone from current glibc).
$(BUILD)/.wayland-stamp: libwayland/protocol/wayland.xml \
		libwayland/src/wayland-version.h.in
	@mkdir -p $(BUILD)/wayland/include
	@printf '%s\n' \
	'/* Autogenerated by the Meson build system. */' \
	'#pragma once' '' \
	'#define HAVE_ACCEPT4' '' \
	'#define HAVE_BROKEN_MSG_CMSG_CLOEXEC 0' '' \
	'#define HAVE_GETTID' '' \
	'#define HAVE_MEMFD_CREATE' '' \
	'#define HAVE_MKOSTEMP' '' \
	'#define HAVE_MREMAP' '' \
	'#define HAVE_POSIX_FALLOCATE' '' \
	'#define HAVE_PRCTL' '' \
	'#define HAVE_STRNDUP' '' \
	'#define HAVE_SYS_PRCTL_H' '' \
	'#undef HAVE_SYS_PROCCTL_H' '' \
	'#undef HAVE_SYS_UCRED_H' '' \
	'#define HAVE_XUCRED_CR_PID 0' '' \
	'#define PACKAGE "wayland"' '' \
	'#define PACKAGE_VERSION "1.26.0"' > $(BUILD)/wayland/config.h
	@sed -e 's/@WAYLAND_VERSION_MAJOR@/1/' \
	    -e 's/@WAYLAND_VERSION_MINOR@/26/' \
	    -e 's/@WAYLAND_VERSION_MICRO@/0/' \
	    -e 's/@WAYLAND_VERSION@/1.26.0/' \
	    libwayland/src/wayland-version.h.in > $(BUILD)/wayland/wayland-version.h
	WAYLAND_SCANNER=$$($(PKG_CONFIG) --variable=wayland_scanner wayland-scanner) && \
	$$WAYLAND_SCANNER server-header libwayland/protocol/wayland.xml \
		$(BUILD)/wayland/wayland-server-protocol.h && \
	$$WAYLAND_SCANNER server-header -c libwayland/protocol/wayland.xml \
		$(BUILD)/wayland/wayland-server-protocol-core.h && \
	$$WAYLAND_SCANNER client-header libwayland/protocol/wayland.xml \
		$(BUILD)/wayland/wayland-client-protocol.h && \
	$$WAYLAND_SCANNER client-header -c libwayland/protocol/wayland.xml \
		$(BUILD)/wayland/wayland-client-protocol-core.h && \
	$$WAYLAND_SCANNER public-code libwayland/protocol/wayland.xml \
		$(BUILD)/wayland/wayland-protocol.c && \
	touch $@

# ---- protocol generation ----

$(BUILD)/.protos-stamp: wlroots/protocol/meson.build
	@mkdir -p $(BUILD)/protocol
	WAYLAND_SCANNER=$$($(PKG_CONFIG) --variable=wayland_scanner wayland-scanner) && \
	WAYLAND_PROTOCOLS=$$($(PKG_CONFIG) --variable=pkgdatadir wayland-protocols) && \
	for xml in \
		$$WAYLAND_PROTOCOLS/stable/linux-dmabuf/linux-dmabuf-v1.xml \
		$$WAYLAND_PROTOCOLS/stable/presentation-time/presentation-time.xml \
		$$WAYLAND_PROTOCOLS/stable/tablet/tablet-v2.xml \
		$$WAYLAND_PROTOCOLS/stable/viewporter/viewporter.xml \
		$$WAYLAND_PROTOCOLS/stable/xdg-shell/xdg-shell.xml \
		$$WAYLAND_PROTOCOLS/staging/alpha-modifier/alpha-modifier-v1.xml \
		$$WAYLAND_PROTOCOLS/staging/color-management/color-management-v1.xml \
		$$WAYLAND_PROTOCOLS/staging/color-representation/color-representation-v1.xml \
		$$WAYLAND_PROTOCOLS/staging/content-type/content-type-v1.xml \
		$$WAYLAND_PROTOCOLS/staging/cursor-shape/cursor-shape-v1.xml \
		$$WAYLAND_PROTOCOLS/staging/drm-lease/drm-lease-v1.xml \
		$$WAYLAND_PROTOCOLS/staging/ext-background-effect/ext-background-effect-v1.xml \
		$$WAYLAND_PROTOCOLS/staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml \
		$$WAYLAND_PROTOCOLS/staging/ext-idle-notify/ext-idle-notify-v1.xml \
		$$WAYLAND_PROTOCOLS/staging/ext-image-capture-source/ext-image-capture-source-v1.xml \
		$$WAYLAND_PROTOCOLS/staging/ext-image-copy-capture/ext-image-copy-capture-v1.xml \
		$$WAYLAND_PROTOCOLS/staging/ext-session-lock/ext-session-lock-v1.xml \
		$$WAYLAND_PROTOCOLS/staging/ext-data-control/ext-data-control-v1.xml \
		$$WAYLAND_PROTOCOLS/staging/ext-workspace/ext-workspace-v1.xml \
		$$WAYLAND_PROTOCOLS/staging/fractional-scale/fractional-scale-v1.xml \
		$$WAYLAND_PROTOCOLS/staging/linux-drm-syncobj/linux-drm-syncobj-v1.xml \
		$$WAYLAND_PROTOCOLS/staging/pointer-warp/pointer-warp-v1.xml \
		$$WAYLAND_PROTOCOLS/staging/security-context/security-context-v1.xml \
		$$WAYLAND_PROTOCOLS/staging/single-pixel-buffer/single-pixel-buffer-v1.xml \
		$$WAYLAND_PROTOCOLS/staging/xdg-activation/xdg-activation-v1.xml \
		$$WAYLAND_PROTOCOLS/staging/xdg-dialog/xdg-dialog-v1.xml \
		$$WAYLAND_PROTOCOLS/staging/xdg-system-bell/xdg-system-bell-v1.xml \
		$$WAYLAND_PROTOCOLS/staging/xdg-toplevel-icon/xdg-toplevel-icon-v1.xml \
		$$WAYLAND_PROTOCOLS/staging/xdg-toplevel-tag/xdg-toplevel-tag-v1.xml \
		$$WAYLAND_PROTOCOLS/staging/xwayland-shell/xwayland-shell-v1.xml \
		$$WAYLAND_PROTOCOLS/staging/tearing-control/tearing-control-v1.xml \
		$$WAYLAND_PROTOCOLS/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml \
		$$WAYLAND_PROTOCOLS/unstable/keyboard-shortcuts-inhibit/keyboard-shortcuts-inhibit-unstable-v1.xml \
		$$WAYLAND_PROTOCOLS/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml \
		$$WAYLAND_PROTOCOLS/unstable/pointer-gestures/pointer-gestures-unstable-v1.xml \
		$$WAYLAND_PROTOCOLS/unstable/primary-selection/primary-selection-unstable-v1.xml \
		$$WAYLAND_PROTOCOLS/unstable/relative-pointer/relative-pointer-unstable-v1.xml \
		$$WAYLAND_PROTOCOLS/unstable/text-input/text-input-unstable-v3.xml \
		$$WAYLAND_PROTOCOLS/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml \
		$$WAYLAND_PROTOCOLS/unstable/xdg-foreign/xdg-foreign-unstable-v1.xml \
		$$WAYLAND_PROTOCOLS/unstable/xdg-foreign/xdg-foreign-unstable-v2.xml \
		$$WAYLAND_PROTOCOLS/unstable/xdg-output/xdg-output-unstable-v1.xml \
		$$WAYLAND_PROTOCOLS/staging/ext-transient-seat/ext-transient-seat-v1.xml \
		wlroots/protocol/drm.xml \
		wlroots/protocol/input-method-unstable-v2.xml \
		wlroots/protocol/server-decoration.xml \
		wlroots/protocol/virtual-keyboard-unstable-v1.xml \
		wlroots/protocol/wlr-data-control-unstable-v1.xml \
		wlroots/protocol/wlr-export-dmabuf-unstable-v1.xml \
		wlroots/protocol/wlr-foreign-toplevel-management-unstable-v1.xml \
		wlroots/protocol/wlr-gamma-control-unstable-v1.xml \
		wlroots/protocol/wlr-layer-shell-unstable-v1.xml \
		wlroots/protocol/wlr-output-management-unstable-v1.xml \
		wlroots/protocol/wlr-output-power-management-unstable-v1.xml \
		wlroots/protocol/wlr-screencopy-unstable-v1.xml \
		wlroots/protocol/wlr-virtual-pointer-unstable-v1.xml; \
	do \
		base=$$(basename "$$xml" .xml) && \
		$$WAYLAND_SCANNER private-code "$$xml" "$(BUILD)/protocol/$$base-protocol.c" || exit 1; \
		$$WAYLAND_SCANNER server-header "$$xml" "$(BUILD)/protocol/$$base-protocol.h" || exit 1; \
	done && \
	for xml in \
		$$WAYLAND_PROTOCOLS/stable/linux-dmabuf/linux-dmabuf-v1.xml \
		$$WAYLAND_PROTOCOLS/stable/presentation-time/presentation-time.xml \
		$$WAYLAND_PROTOCOLS/stable/tablet/tablet-v2.xml \
		$$WAYLAND_PROTOCOLS/stable/viewporter/viewporter.xml \
		$$WAYLAND_PROTOCOLS/stable/xdg-shell/xdg-shell.xml \
		$$WAYLAND_PROTOCOLS/staging/linux-drm-syncobj/linux-drm-syncobj-v1.xml \
		$$WAYLAND_PROTOCOLS/staging/xdg-activation/xdg-activation-v1.xml \
		$$WAYLAND_PROTOCOLS/unstable/pointer-gestures/pointer-gestures-unstable-v1.xml \
		$$WAYLAND_PROTOCOLS/unstable/relative-pointer/relative-pointer-unstable-v1.xml \
		$$WAYLAND_PROTOCOLS/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml \
		wlroots/protocol/drm.xml; \
	do \
		base=$$(basename "$$xml" .xml) && \
		$$WAYLAND_SCANNER client-header "$$xml" "$(BUILD)/protocol/$$base-client-protocol.h" || exit 1; \
	done && \
	touch $@

# ---- shader generation ----

SHADER_INPUTS = \
	wlroots/render/gles2/shaders/common.vert \
	wlroots/render/gles2/shaders/quad.frag \
	wlroots/render/gles2/shaders/tex_rgba.frag \
	wlroots/render/gles2/shaders/tex_rgbx.frag \
	wlroots/render/gles2/shaders/tex_external.frag

$(BUILD)/.shaders-stamp: $(SHADER_INPUTS) wlroots/render/gles2/shaders/embed.sh
	@mkdir -p $(BUILD)/shaders
	sh wlroots/render/gles2/shaders/embed.sh common_vert_src < wlroots/render/gles2/shaders/common.vert > $(BUILD)/shaders/common_vert_src.h
	sh wlroots/render/gles2/shaders/embed.sh quad_frag_src < wlroots/render/gles2/shaders/quad.frag > $(BUILD)/shaders/quad_frag_src.h
	sh wlroots/render/gles2/shaders/embed.sh tex_rgba_frag_src < wlroots/render/gles2/shaders/tex_rgba.frag > $(BUILD)/shaders/tex_rgba_frag_src.h
	sh wlroots/render/gles2/shaders/embed.sh tex_rgbx_frag_src < wlroots/render/gles2/shaders/tex_rgbx.frag > $(BUILD)/shaders/tex_rgbx_frag_src.h
	sh wlroots/render/gles2/shaders/embed.sh tex_external_frag_src < wlroots/render/gles2/shaders/tex_external.frag > $(BUILD)/shaders/tex_external_frag_src.h
	@touch $@

# ---- pnpids generation ----

$(BUILD)/backend/drm/pnpids.c: wlroots/backend/drm/gen_pnpids.sh
	@mkdir -p $(BUILD)/backend/drm
	sh wlroots/backend/drm/gen_pnpids.sh < /usr/share/hwdata/pnp.ids > $@

# ---- compile rules ----

# generated files that do not exist at parse time — make knows they will
# appear after the stamp target runs
$(BUILD)/protocol/%-protocol.c $(BUILD)/protocol/%-protocol.h \
	$(BUILD)/protocol/%-client-protocol.h: $(BUILD)/.protos-stamp
	@true
$(BUILD)/wayland/wayland-protocol.c: $(BUILD)/.wayland-stamp
	@true
$(BUILD)/shaders/%.h: $(BUILD)/.shaders-stamp
	@true

# wlroots source objects
$(BUILD)/%.o: wlroots/%.c $(CONFIG_HEADERS) | $(BUILD)/.protos-stamp $(BUILD)/.shaders-stamp $(BUILD)/pixman/pixman-version.h $(BUILD)/.wayland-stamp
	@mkdir -p $(@D)
	$(CC) $(WLR_CFLAGS) -c $< -o $@

# protocol objects — .c files are generated by the stamp target
$(BUILD)/protocol/%.o: $(BUILD)/.protos-stamp $(CONFIG_HEADERS)
	@mkdir -p $(@D)
	$(CC) $(WLR_CFLAGS) -c $(BUILD)/protocol/$*.c -o $@

# libwayland source objects
$(BUILD)/libwayland/%.o: libwayland/src/%.c | $(BUILD)/.wayland-stamp
	@mkdir -p $(@D)
	$(CC) $(WAYLAND_CFLAGS) -c $< -o $@

# core protocol marshalling code — shared by libwayland-server and -client
$(BUILD)/wayland/wayland-protocol.o: $(BUILD)/wayland/wayland-protocol.c
	@mkdir -p $(@D)
	$(CC) $(WAYLAND_CFLAGS) -c $< -o $@

# pnpids
$(BUILD)/backend/drm/pnpids.o: $(BUILD)/backend/drm/pnpids.c $(CONFIG_HEADERS)
	@mkdir -p $(@D)
	$(CC) $(WLR_CFLAGS) -c $< -o $@

# libliftoff
$(BUILD)/libliftoff/%.o: libliftoff/%.c
	@mkdir -p $(@D)
	$(CC) -Ilibliftoff/include -I$(BUILD)/include \
		`$(PKG_CONFIG) --cflags libdrm` $(CFLAGS) -c $< -o $@

# libdisplay-info — generated PNP ID search table from hwdata
$(BUILD)/libdisplay-info/pnp-id-table.c: libdisplay-info/tool/gen-search-table.py
	@mkdir -p $(BUILD)/libdisplay-info
	python3 libdisplay-info/tool/gen-search-table.py \
		/usr/share/hwdata/pnp.ids $@ pnp_id_table

$(BUILD)/libdisplay-info/%.o: libdisplay-info/%.c
	@mkdir -p $(@D)
	$(CC) -Ilibdisplay-info/include $(CFLAGS) -D_POSIX_C_SOURCE=200809L -c $< -o $@

# ---- libinput ----

# Stands in for libinput's meson configure_file(output: 'config.h'). Every
# define below mirrors a probe in libinput/meson.build, run for this machine:
# glibc 2.43 (versionsort, sigabbrev_np, locale.h), kernel headers carrying
# SYS_pidfd_open, gcc 15 accepting the C23 auto keyword, and no xlocale.h.
#
# Left out on purpose, matching the system libinput 1.31.3 this replaces:
# HAVE_LUA/HAVE_PLUGINS/AUTOLOAD_PLUGINS (no Lua, so plugins are never
# dlopen()ed), HAVE_MTDEV (protocol A multitouch), HAVE_LIBWACOM (tablet quirks
# keyed on Wacom IDs), HAVE_XLOCALE_H, IS_DEBUG_BUILD (asserts stay live for
# libinput - $(CFLAGS) has no -DNDEBUG).
$(BUILD)/libinput/config.h: libinput/meson.build
	@mkdir -p $(@D)
	@printf '%s\n' \
		'/* generated by the Makefile; see libinput/meson.build */' \
		'#define _GNU_SOURCE 1' \
		'' \
		'/* <assert.h> here has no static_assert, so fold it away */' \
		'#define static_assert(...) /* */' \
		'' \
		'#define HAVE_VERSIONSORT 1' \
		'#define HAVE_PIDFD_OPEN 1' \
		'#define HAVE_SIGABBREV_NP 1' \
		'#define HAVE_LOCALE_H 1' \
		'#define HAVE_C23_AUTO 1' \
		'' \
		'#define HTTP_DOC_LINK "https://wayland.freedesktop.org/libinput/doc/$(LIBINPUT_VERSION)"' \
		'' \
		'/* Quirks come from the vendored tree: jtl installs no libinput' \
		' * data files, so LIBINPUT_QUIRKS_SRCDIR is the fallback path. */' \
		'#define LIBINPUT_QUIRKS_DIR "$(CURDIR)/libinput/quirks"' \
		'#define LIBINPUT_QUIRKS_SRCDIR "$(CURDIR)/libinput/quirks"' \
		'#define LIBINPUT_QUIRKS_OVERRIDE_FILE "/etc/libinput/local-overrides.quirk"' \
		'' \
		'/* Only ever read with HAVE_PLUGINS, but referenced unconditionally */' \
		'#define LIBINPUT_PLUGIN_LIBDIR "/usr/local/lib/libinput/plugins"' \
		'#define LIBINPUT_PLUGIN_ETCDIR "/etc/libinput/plugins"' \
		> $@

$(BUILD)/libinput/include/libinput-version.h: libinput/src/libinput-version.h.in \
		libinput/meson.build
	@mkdir -p $(@D)
	sed -e 's/@LIBINPUT_VERSION_MAJOR@/$(LIBINPUT_VER_MAJOR)/' \
	    -e 's/@LIBINPUT_VERSION_MINOR@/$(LIBINPUT_VER_MINOR)/' \
	    -e 's/@LIBINPUT_VERSION_MICRO@/$(LIBINPUT_VER_MICRO)/' \
	    -e 's/@LIBINPUT_VERSION@/$(LIBINPUT_VERSION)/' $< > $@

$(BUILD)/libinput/include/libinput.h: libinput/src/libinput.h
	@mkdir -p $(@D)
	ln -sf $(CURDIR)/libinput/src/libinput.h $@

$(BUILD)/libinput/src/%.o: libinput/src/%.c $(LIBINPUT_GEN)
	@mkdir -p $(@D)
	$(CC) $(LIBINPUT_CFLAGS) -c $< -o $@

# ---- xkbcommon ----

# The XKBCOMMON_* variables live with the libinput ones above, before the jtl
# rule: make expands a prerequisite list when it reads the rule, so a variable
# defined further down would leave the objects off the prerequisite list while
# still putting them on the link line.

# config.h mirrors the configh_data block in xkbcommon/meson.build (lines
# 166-305). Every HAVE_* below was probed with the compiler on this host rather
# than assumed: glibc 2.43 and GCC 15 have all of them, except __secure_getenv,
# which this glibc does not declare, so neither HAVE_SECURE_GETENV nor
# HAVE___SECURE_GETENV is defined (meson tests the __-prefixed one first).
#
# The XKB data paths are what meson would bake in from the xkeyboard-config
# pkg-config file. They follow the data on this machine (/usr/share/X11/xkb,
# Gentoo prefix /usr) rather than meson's own /usr/local defaults: jtl reads the
# system xkeyboard-config, so the extensions and locale paths have to sit
# beside it too. DFLT_XKB_CONFIG_UNVERSIONED_EXTENSIONS_PATH is meson's
# prefix/datadir fallback (meson.build:125-142) and DFLT_XKB_CONFIG_EXTRA_PATH is
# its sysconfdir fallback, both moved from /usr/local to /usr for that reason.
# LIBXKBCOMMON_TOOL_PATH only matters to xkbcli, which is not built here.
#
# Not defined, as upstream leaves them: the HAVE_XML_* and HAVE_ICU probes
# (xkbcommon-x11 only), and every HAVE_XKBCLI_* / HAVE_TOOLS flag (tools).
$(BUILD)/xkbcommon/config.h: xkbcommon/meson.build xkbcommon/meson_options.txt
	@mkdir -p $(@D)
	@printf '%s\n' \
		'/* generated by the Makefile; see xkbcommon/meson.build */' \
		'#define _GNU_SOURCE 1' \
		'' \
		'#define EXIT_INVALID_USAGE 2' \
		'#define LIBXKBCOMMON_VERSION "$(XKBCOMMON_VERSION)"' \
		'#define LIBXKBCOMMON_TOOL_PATH "/usr/local/libexec/xkbcommon"' \
		'' \
		'#define DFLT_XKB_LEGACY_ROOT ""' \
		'#define DFLT_XKB_CONFIG_ROOT "/usr/share/X11/xkb"' \
		'#define DFLT_XKB_CONFIG_UNVERSIONED_EXTENSIONS_PATH "/usr/share/xkeyboard-config.d"' \
		'#define DFLT_XKB_CONFIG_VERSIONED_EXTENSIONS_PATH "/usr/share/X11/xkb.d"' \
		'#define DFLT_XKB_CONFIG_EXTRA_PATH "/etc/xkb"' \
		'#define XLOCALEDIR "/usr/share/X11/locale"' \
		'' \
		'#define DEFAULT_XKB_RULES "evdev"' \
		'#define DEFAULT_XKB_MODEL "pc105"' \
		'#define DEFAULT_XKB_LAYOUT "us"' \
		'#define DEFAULT_XKB_VARIANT NULL' \
		'#define DEFAULT_XKB_OPTIONS NULL' \
		'' \
		'#define HAVE_UNISTD_H 1' \
		'#define HAVE_DIRENT_H 1' \
		'#define HAVE_XKB_EXTENSIONS_DIRECTORIES 1' \
		'#define HAVE___BUILTIN_EXPECT 1' \
		'#define HAVE_EACCESS 1' \
		'#define HAVE_EUIDACCESS 1' \
		'#define HAVE_MMAP 1' \
		'#define HAVE_MKOSTEMP 1' \
		'#define HAVE_POSIX_FALLOCATE 1' \
		'#define HAVE_STRNDUP 1' \
		'#define HAVE_ASPRINTF 1' \
		'#define HAVE_VASPRINTF 1' \
		'#define HAVE_OPEN_MEMSTREAM 1' \
		'#define HAVE_REAL_PATH 1' \
		'#define HAVE_NEWLOCALE 1' \
		'#define PATH_MAX 4096' \
		> $@

# meson.build:322-326 generates the xkbcomp parser with bison, prefix
# _xkbcommon_ so its symbols cannot collide with the compose parser.
$(XKBCOMMON_PARSER) $(XKBCOMMON_PARSER_H) &: xkbcommon/src/xkbcomp/parser.y
	@mkdir -p $(@D)
	bison --defines=$(XKBCOMMON_PARSER_H) -o $(XKBCOMMON_PARSER) \
		-p _xkbcommon_ $<

$(BUILD)/xkbcommon/include/xkbcommon/%.h: xkbcommon/include/xkbcommon/%.h
	@mkdir -p $(@D)
	ln -sf $(CURDIR)/$< $@

$(BUILD)/xkbcommon/src/%.o: xkbcommon/src/%.c $(XKBCOMMON_GEN)
	@mkdir -p $(@D)
	$(CC) $(XKBCOMMON_CFLAGS) -c $< -o $@

$(XKBCOMMON_PARSER_OBJ): $(XKBCOMMON_PARSER) $(XKBCOMMON_GEN)
	@mkdir -p $(@D)
	$(CC) $(XKBCOMMON_CFLAGS) -c $< -o $@

# ---- pixman ----

PIXMAN_CFLAGS = -I$(BUILD)/pixman -Ipixman -DHAVE_CONFIG_H \
	-fno-strict-aliasing -ftrapping-math $(CFLAGS)

$(BUILD)/pixman/pixman-config.h:
	@mkdir -p $(BUILD)/pixman
	@printf '%s\n' \
	'#define USE_X86_MMX 1' \
	'#define USE_SSE2 1' \
	'#define USE_SSSE3 1' \
	'#define USE_GCC_INLINE_ASM 1' \
	'#define HAVE_PTHREADS 1' \
	'#define TLS __thread' \
	'#define HAVE_SIGACTION 1' \
	'#define HAVE_ALARM 1' \
	'#define HAVE_MPROTECT 1' \
	'#define HAVE_GETPAGESIZE 1' \
	'#define HAVE_MMAP 1' \
	'#define HAVE_GETTIMEOFDAY 1' \
	'#define HAVE_POSIX_MEMALIGN 1' \
	'#define HAVE_SYS_MMAN_H 1' \
	'#define HAVE_FENV_H 1' \
	'#define HAVE_UNISTD_H 1' \
	'#define HAVE_FEENABLEEXCEPT 1' \
	'#define HAVE_FEDIVBYZERO 1' \
	'#define TOOLCHAIN_SUPPORTS_ATTRIBUTE_CONSTRUCTOR 1' \
	'#define TOOLCHAIN_SUPPORTS_ATTRIBUTE_DESTRUCTOR 1' \
	'#define HAVE_FLOAT128 1' \
	'#define HAVE_GCC_VECTOR_EXTENSIONS 1' \
	'#define HAVE_BUILTIN_CLZ 1' \
	'#define SIZEOF_LONG 8' \
	'#define PACKAGE "foo"' > $@

$(BUILD)/pixman/pixman-version.h: pixman/pixman-version.h.in
	@mkdir -p $(BUILD)/pixman
	sed -e 's/@PIXMAN_VERSION_MAJOR@/0/' \
	    -e 's/@PIXMAN_VERSION_MINOR@/46/' \
	    -e 's/@PIXMAN_VERSION_MICRO@/4/' $< > $@

$(BUILD)/pixman/%.o: pixman/%.c $(PIXMAN_CONFIG)
	@mkdir -p $(@D)
	$(CC) $(PIXMAN_CFLAGS) -c $< -o $@

$(BUILD)/pixman/pixman-mmx.o: pixman/pixman-mmx.c $(PIXMAN_CONFIG)
	@mkdir -p $(@D)
	$(CC) $(PIXMAN_CFLAGS) -mmmx -c $< -o $@

$(BUILD)/pixman/pixman-sse2.o: pixman/pixman-sse2.c $(PIXMAN_CONFIG)
	@mkdir -p $(@D)
	$(CC) $(PIXMAN_CFLAGS) -msse2 -c $< -o $@

$(BUILD)/pixman/pixman-ssse3.o: pixman/pixman-ssse3.c $(PIXMAN_CONFIG)
	@mkdir -p $(@D)
	$(CC) $(PIXMAN_CFLAGS) -mssse3 -c $< -o $@

# jtl.o
jtl.o: jtl.c $(CONFIG_HEADERS) | $(BUILD)/.protos-stamp $(BUILD)/pixman/pixman-version.h $(BUILD)/.wayland-stamp
	$(CC) $(DWLCFLAGS) -c $< -o $@

# ---- clean / install / dist ----

# The Mesa sub-build takes ~15 minutes, so it survives a plain `make clean`;
# `make clean mesa` throws it away as well. Since that spells two goals, the
# mesa build target below is skipped whenever clean is one of them - otherwise
# make would build Mesa again right after cleaning it.
CLEAN_MESA := $(filter mesa,$(MAKECMDGOALS))
# $(wildcard) skips dotfiles, and the build stamps in build/ are dotfiles, so
# clean goes through find and leaves only the Mesa sub-build alone.
ifeq ($(CLEAN_MESA),)
CLEAN_MESA_EXCLUDE = ! -name $(notdir $(MESA_BUILD))
else
CLEAN_MESA_EXCLUDE =
endif

clean:
	rm -f jtl jtl.o
	find $(BUILD) -mindepth 1 -maxdepth 1 $(CLEAN_MESA_EXCLUDE) -exec rm -rf {} +

dist: clean
	mkdir -p jtl-$(VERSION)
	cp -R LICENSE* Makefile CHANGELOG.md README.md config.def.h \
		config.mk protocols jtl.c jtl.desktop \
		wlroots.build-files.txt wlroots libliftoff libdisplay-info pixman \
		libwayland mesa jtl-$(VERSION)
	tar -caf jtl-$(VERSION).tar.gz jtl-$(VERSION)
	rm -rf jtl-$(VERSION)

install: jtl
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	rm -f $(DESTDIR)$(PREFIX)/bin/jtl
	cp -f jtl $(DESTDIR)$(PREFIX)/bin
	$(STRIP) $(DESTDIR)$(PREFIX)/bin/jtl
	chmod 755 $(DESTDIR)$(PREFIX)/bin/jtl
	mkdir -p $(DESTDIR)$(DATADIR)/wayland-sessions
	cp -f jtl.desktop $(DESTDIR)$(DATADIR)/wayland-sessions/jtl.desktop
	chmod 644 $(DESTDIR)$(DATADIR)/wayland-sessions/jtl.desktop

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/jtl \
		$(DESTDIR)$(DATADIR)/wayland-sessions/jtl.desktop

.PHONY: all clean dist install uninstall mesa
