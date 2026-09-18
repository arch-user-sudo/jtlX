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
CONFIG_HEADERS = $(BUILD)/include/config.h $(BUILD)/include/wlr/config.h $(BUILD)/include/wlr/version.h
ALL_OBJS = $(WLR_OBJ) $(PROTO_OBJ) $(PNPID_OBJ) $(LIFTOFF_OBJ) $(DI_OBJ) \
	$(PIXMAN_COM_OBJ) $(PIXMAN_SIMD_OBJ)

# PKGS used by both compilation and linking (wlroots, libliftoff, libdisplay-info
# and pixman are vendored, so their own .pc files are no longer needed)
PKGS = wayland-server wayland-client xkbcommon libinput libdrm \
	libseat libudev gbm egl glesv2 $(XLIBS)

# wlroots compile flags
WLRDEFS = -D_POSIX_C_SOURCE=200809L -DWLR_USE_UNSTABLE -DWLR_PRIVATE= \
	-DWLR_LITTLE_ENDIAN=1 -DWLR_BIG_ENDIAN=0
WLRWARN = -Wundef -Wlogical-op -Wmissing-include-dirs -Wold-style-definition \
	-Wpointer-arith -Winit-self -Wstrict-prototypes \
	-Wimplicit-fallthrough=2 -Wendif-labels -Wstrict-aliasing=2 \
	-Woverflow -Wmissing-prototypes -Walloca \
	-Wno-missing-braces -Wno-missing-field-initializers -Wno-unused-parameter
WLRINCS = -I$(BUILD)/include -Iwlroots/include -I$(BUILD)/protocol -I$(BUILD)/shaders \
	-Ilibliftoff/include -Ilibdisplay-info/include -Ipixman -I$(BUILD)/pixman \
	`$(PKG_CONFIG) --cflags $(PKGS)`
WLR_CFLAGS = $(WLRDEFS) $(WLRWARN) $(WLRINCS) $(CFLAGS)

# jtl.o compile flags
DWLCPPFLAGS = -I. -DWLR_USE_UNSTABLE -D_POSIX_C_SOURCE=200809L \
	-DVERSION=\"$(VERSION)\" $(XWAYLAND) $(FULLSCREEN_TEARING) \
	$(FOREIGN_TOPLEVEL) $(WORKSPACES) \
	-DWLR_PRIVATE= -DWLR_LITTLE_ENDIAN=1 -DWLR_BIG_ENDIAN=0
DWLDEVCFLAGS = -g -Wpedantic -Wall -Wextra -Wdeclaration-after-statement \
	-Wno-unused-parameter -Wshadow -Wunused-macros -Werror=strict-prototypes \
	-Werror=implicit -Werror=return-type -Werror=incompatible-pointer-types \
	-Wfloat-conversion -Wimplicit-fallthrough
DWLCFLAGS = -Ipixman -I$(BUILD)/pixman \
	`$(PKG_CONFIG) --cflags $(PKGS)` -I$(BUILD)/include -Iwlroots/include \
	-I$(BUILD)/protocol $(DWLCPPFLAGS) $(DWLDEVCFLAGS) $(CFLAGS)

LDLIBS = `$(PKG_CONFIG) --libs $(PKGS)` -lm

all: jtl

jtl: jtl.o $(ALL_OBJS)
	$(CC) $^ $(LDFLAGS) $(LDLIBS) -o $@

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
$(BUILD)/shaders/%.h: $(BUILD)/.shaders-stamp
	@true

# wlroots source objects
$(BUILD)/%.o: wlroots/%.c $(CONFIG_HEADERS) | $(BUILD)/.protos-stamp $(BUILD)/.shaders-stamp $(BUILD)/pixman/pixman-version.h
	@mkdir -p $(@D)
	$(CC) $(WLR_CFLAGS) -c $< -o $@

# protocol objects — .c files are generated by the stamp target
$(BUILD)/protocol/%.o: $(BUILD)/.protos-stamp $(CONFIG_HEADERS)
	@mkdir -p $(@D)
	$(CC) $(WLR_CFLAGS) -c $(BUILD)/protocol/$*.c -o $@

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
jtl.o: jtl.c $(CONFIG_HEADERS) | $(BUILD)/.protos-stamp $(BUILD)/pixman/pixman-version.h
	$(CC) $(DWLCFLAGS) -c $< -o $@

# ---- clean / install / dist ----

clean:
	rm -f jtl jtl.o
	rm -rf $(BUILD)

dist: clean
	mkdir -p jtl-$(VERSION)
	cp -R LICENSE* Makefile CHANGELOG.md README.md config.def.h \
		config.mk protocols jtl.c jtl.desktop \
		wlroots.build-files.txt wlroots libliftoff libdisplay-info pixman jtl-$(VERSION)
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

.PHONY: all clean dist install uninstall
