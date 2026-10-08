#!/usr/bin/env python3
"""Generate wlroots.c - all of wlroots inlined into one self-contained file.

    jtl.c (hand-written)  --  #include "wlroots.c"  -->  wlroots.c (generated)

Run it while the vendored wlroots/ tree is still here:

    python3 amalgamate.py            # or: make amalgamate

Afterwards wlroots/ can be deleted: building only needs jtl.c, wlroots.c,
config.h and config.mk.  `make` regenerates wlroots.c when jtl.c or config.mk
change (as long as wlroots/ exists; otherwise the existing wlroots.c is kept).

wlroots itself is never compiled - the only tools used are:

  * wayland-scanner, for the generated protocol code,
  * two small sh scripts vendored in wlroots/ (shader headers, pnp.ids table),
  * the C preprocessor, to see which wlroots symbols jtl.c actually uses,
  * glslang, only when config.mk enables VULKAN (SPIR-V shaders).

Which wlroots features go in follows config.mk: every -D option found there
(XWAYLAND, X11_BACKEND, VULKAN, ...) pulls in the matching wlroots sources and
sets the corresponding WLR_HAS_* macros in the generated config.h, so toggling
a feature in config.mk re-pulls exactly the right sources.
"""

import os
import re
import sys
import shutil
import subprocess

sys.dont_write_bytecode = True
import scan  # noqa: E402  (same directory as this script)

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = HERE
WLR = os.path.join(ROOT, 'wlroots')
GEN = os.path.join(ROOT, 'build')          # generated support files (protocol,
                                           # shaders, config headers, pnpids)
JTL = os.path.join(ROOT, 'jtl.c')
CONFIG_MK = os.path.join(ROOT, 'config.mk')
OUT = os.path.join(ROOT, 'wlroots.c')

# --------------------------------------------------------------------------
# what to pull in
# --------------------------------------------------------------------------

# Every wlroots source that is part of this vendored tree and does not depend
# on an optional feature (the list meson produced for wlroots/ at vendoring
# time).  Files whose compilation is controlled by config.mk are below.
SOURCES = ['backend/backend.c', 'backend/drm/atomic.c', 'backend/drm/backend.c', 'backend/drm/drm.c', 'backend/drm/fb.c', 'backend/drm/legacy.c', 'backend/drm/libliftoff.c', 'backend/drm/monitor.c', 'backend/drm/properties.c', 'backend/drm/renderer.c', 'backend/drm/util.c', 'backend/headless/backend.c', 'backend/headless/output.c', 'backend/libinput/backend.c', 'backend/libinput/events.c', 'backend/libinput/keyboard.c', 'backend/libinput/pointer.c', 'backend/libinput/switch.c', 'backend/libinput/tablet_pad.c', 'backend/libinput/tablet_tool.c', 'backend/libinput/touch.c', 'backend/multi/backend.c', 'backend/session/session.c', 'backend/wayland/backend.c', 'backend/wayland/output.c', 'backend/wayland/pointer.c', 'backend/wayland/seat.c', 'backend/wayland/tablet_v2.c', 'render/allocator/allocator.c', 'render/allocator/drm_dumb.c', 'render/allocator/gbm.c', 'render/allocator/shm.c', 'render/allocator/udmabuf.c', 'render/color.c', 'render/color_fallback.c', 'render/dmabuf.c', 'render/dmabuf_linux.c', 'render/drm_format_set.c', 'render/drm_syncobj.c', 'render/drm_syncobj_merger.c', 'render/egl.c', 'render/gles2/pass.c', 'render/gles2/pixel_format.c', 'render/gles2/renderer.c', 'render/gles2/texture.c', 'render/pass.c', 'render/pixel_format.c', 'render/pixel_format_table.c', 'render/pixman/pass.c', 'render/pixman/pixel_format.c', 'render/pixman/renderer.c', 'render/swapchain.c', 'render/wlr_renderer.c', 'render/wlr_texture.c', 'types/buffer/buffer.c', 'types/buffer/client.c', 'types/buffer/dmabuf.c', 'types/buffer/readonly_data.c', 'types/buffer/resource.c', 'types/data_device/wlr_data_device.c', 'types/data_device/wlr_data_offer.c', 'types/data_device/wlr_data_source.c', 'types/data_device/wlr_drag.c', 'types/ext_image_capture_source_v1/base.c', 'types/ext_image_capture_source_v1/foreign_toplevel.c', 'types/ext_image_capture_source_v1/output.c', 'types/ext_image_capture_source_v1/scene.c', 'types/output/cursor.c', 'types/output/output.c', 'types/output/render.c', 'types/output/state.c', 'types/output/swapchain.c', 'types/scene/drag_icon.c', 'types/scene/layer_shell_v1.c', 'types/scene/output_layout.c', 'types/scene/subsurface_tree.c', 'types/scene/surface.c', 'types/scene/wlr_scene.c', 'types/scene/xdg_shell.c', 'types/seat/wlr_seat.c', 'types/seat/wlr_seat_keyboard.c', 'types/seat/wlr_seat_pointer.c', 'types/seat/wlr_seat_touch.c', 'types/tablet_v2/wlr_tablet_v2.c', 'types/tablet_v2/wlr_tablet_v2_pad.c', 'types/tablet_v2/wlr_tablet_v2_tablet.c', 'types/tablet_v2/wlr_tablet_v2_tool.c', 'types/wlr_alpha_modifier_v1.c', 'types/wlr_color_management_v1.c', 'types/wlr_color_representation_v1.c', 'types/wlr_compositor.c', 'types/wlr_content_type_v1.c', 'types/wlr_cursor.c', 'types/wlr_cursor_shape_v1.c', 'types/wlr_damage_ring.c', 'types/wlr_data_control_v1.c', 'types/wlr_drm.c', 'types/wlr_drm_lease_v1.c', 'types/wlr_export_dmabuf_v1.c', 'types/wlr_ext_background_effect_v1.c', 'types/wlr_ext_data_control_v1.c', 'types/wlr_ext_foreign_toplevel_list_v1.c', 'types/wlr_ext_image_copy_capture_v1.c', 'types/wlr_ext_workspace_v1.c', 'types/wlr_fixes.c', 'types/wlr_foreign_toplevel_management_v1.c', 'types/wlr_fractional_scale_v1.c', 'types/wlr_gamma_control_v1.c', 'types/wlr_idle_inhibit_v1.c', 'types/wlr_idle_notify_v1.c', 'types/wlr_input_device.c', 'types/wlr_input_method_v2.c', 'types/wlr_keyboard.c', 'types/wlr_keyboard_group.c', 'types/wlr_keyboard_shortcuts_inhibit_v1.c', 'types/wlr_layer_shell_v1.c', 'types/wlr_linux_dmabuf_v1.c', 'types/wlr_linux_drm_syncobj_v1.c', 'types/wlr_output_layer.c', 'types/wlr_output_layout.c', 'types/wlr_output_management_v1.c', 'types/wlr_output_power_management_v1.c', 'types/wlr_output_swapchain_manager.c', 'types/wlr_pointer.c', 'types/wlr_pointer_constraints_v1.c', 'types/wlr_pointer_gestures_v1.c', 'types/wlr_pointer_warp_v1.c', 'types/wlr_presentation_time.c', 'types/wlr_primary_selection.c', 'types/wlr_primary_selection_v1.c', 'types/wlr_region.c', 'types/wlr_relative_pointer_v1.c', 'types/wlr_screencopy_v1.c', 'types/wlr_security_context_v1.c', 'types/wlr_server_decoration.c', 'types/wlr_session_lock_v1.c', 'types/wlr_shm.c', 'types/wlr_single_pixel_buffer_v1.c', 'types/wlr_subcompositor.c', 'types/wlr_switch.c', 'types/wlr_tablet_pad.c', 'types/wlr_tablet_tool.c', 'types/wlr_tearing_control_v1.c', 'types/wlr_text_input_v3.c', 'types/wlr_touch.c', 'types/wlr_transient_seat_v1.c', 'types/wlr_viewporter.c', 'types/wlr_virtual_keyboard_v1.c', 'types/wlr_virtual_pointer_v1.c', 'types/wlr_xcursor_manager.c', 'types/wlr_xdg_activation_v1.c', 'types/wlr_xdg_decoration_v1.c', 'types/wlr_xdg_dialog_v1.c', 'types/wlr_xdg_foreign_registry.c', 'types/wlr_xdg_foreign_v1.c', 'types/wlr_xdg_foreign_v2.c', 'types/wlr_xdg_output_v1.c', 'types/wlr_xdg_system_bell_v1.c', 'types/wlr_xdg_toplevel_icon_v1.c', 'types/wlr_xdg_toplevel_tag_v1.c', 'types/xdg_shell/wlr_xdg_popup.c', 'types/xdg_shell/wlr_xdg_positioner.c', 'types/xdg_shell/wlr_xdg_shell.c', 'types/xdg_shell/wlr_xdg_surface.c', 'types/xdg_shell/wlr_xdg_toplevel.c', 'util/addon.c', 'util/array.c', 'util/box.c', 'util/env.c', 'util/fd.c', 'util/global.c', 'util/log.c', 'util/matrix.c', 'util/mem.c', 'util/rect_union.c', 'util/region.c', 'util/set.c', 'util/shm.c', 'util/time.c', 'util/token.c', 'util/transform.c', 'util/utf8.c', 'util/version.c', 'xcursor/wlr_xcursor.c', 'xcursor/xcursor.c']

# Pulled in only when config.mk defines XWAYLAND.
XWAYLAND_SOURCES = [
    'xwayland/selection/dnd.c',
    'xwayland/selection/incoming.c',
    'xwayland/selection/outgoing.c',
    'xwayland/selection/selection.c',
    'xwayland/server.c',
    'xwayland/shell.c',
    'xwayland/sockets.c',
    'xwayland/xwayland.c',
    'xwayland/xwm.c',
]

# Pulled in only when config.mk defines X11_BACKEND (the X11 backend; named
# so because jtl.c uses `X11` as an enum value).  Its deps are just the xcb-*
# libraries, all already in PKGS.
X11_SOURCES = [
    'backend/x11/backend.c',
    'backend/x11/input_device.c',
    'backend/x11/output.c',
]

# Pulled in only when config.mk defines VULKAN (the Vulkan renderer).  Needs
# vulkan-loader + vulkan-headers, and gen_vulkan_shaders() needs glslang.
VULKAN_SOURCES = [
    'render/vulkan/pass.c',
    'render/vulkan/pixel_format.c',
    'render/vulkan/renderer.c',
    'render/vulkan/texture.c',
    'render/vulkan/util.c',
    'render/vulkan/vulkan.c',
]

# SPIR-V shaders compiled to C headers (mirrors render/vulkan/shaders/meson.build).
VULKAN_SHADERS = ['common.vert', 'texture.frag', 'quad.frag', 'output.frag']

# GLSL shaders embedded as C headers (mirrors wlroots' shaders/meson.build).
SHADERS = ['common.vert', 'quad.frag', 'tex_rgba.frag', 'tex_rgbx.frag',
           'tex_external.frag']

# The two feature headers meson used to generate; regenerated here verbatim.
WLR_CONFIG_H = """\
#ifndef WLR_CONFIG_H
#define WLR_CONFIG_H

/**
 * Whether the DRM backend is compile-time enabled. Equivalent to the
 * pkg-config "have_drm_backend" variable.
 *
 * Required for <wlr/backend/drm.h>.
 */
#define WLR_HAS_DRM_BACKEND 1
/**
 * Whether the libinput backend is compile-time enabled. Equivalent to the
 * pkg-config "have_libinput_backend" variable.
 *
 * Required for <wlr/backend/libinput.h>.
 */
#define WLR_HAS_LIBINPUT_BACKEND 1
/**
 * Whether the X11 backend is compile-time enabled. Equivalent to the
 * pkg-config "have_x11_backend" variable.
 *
 * Required for <wlr/backend/x11.h>.
 */
#define WLR_HAS_X11_BACKEND {X11}

/**
 * Whether the GLES2 renderer is compile-time enabled. Equivalent to the
 * pkg-config "have_gles2_renderer" variable.
 *
 * Required for <wlr/render/gles2.h>.
 */
#define WLR_HAS_GLES2_RENDERER 1
/**
 * Whether the Vulkan renderer is compile-time enabled. Equivalent to the
 * pkg-config "have_vulkan_renderer" variable.
 *
 * Required for <wlr/render/vulkan.h>.
 */
#define WLR_HAS_VULKAN_RENDERER {VULKAN}

/**
 * Whether the GBM allocator is compile-time enabled. Equivalent to the
 * pkg-config "have_gbm_allocator" variable.
 */
#define WLR_HAS_GBM_ALLOCATOR 1
/**
 * Whether the udmabuf allocator is compile-time enabled. Equivalent to the
 * pkg-config "have_udmabuf_allocator" variable.
 */
#define WLR_HAS_UDMABUF_ALLOCATOR 1

/**
 * Whether Xwayland support is compile-time enabled. Equivalent to the
 * pkg-config "have_xwayland" variable.
 *
 * Required for <wlr/xwayland/...>.
 */
#define WLR_HAS_XWAYLAND {XWAYLAND}

/**
 * Whether session support is compile-time enabled. Equivalent to the
 * pkg-config "have_session" variable.
 *
 * Required for <wlr/backend/session.h>.
 */
#define WLR_HAS_SESSION 1

/**
 * Whether traditional color management support is compile-time enabled.
 * Equivalent to the pkg-config "have_color_management" variable.
 *
 * Required for ICC profile support in <wlr/render/color.h>.
 */
/* #undef WLR_HAS_COLOR_MANAGEMENT */

#endif
"""

INTERNAL_CONFIG_H = """\
/*
 * Generated by amalgamate.py for the wlroots code inlined in wlroots.c.
 */

#define HAVE_EGL 1

#define HAVE_EVENTFD 1

#define HAVE_LIBINPUT_BUSTYPE 1

#define HAVE_LIBINPUT_SWITCH_KEYPAD_SLIDE 1

#define HAVE_LIBLIFTOFF 1

#define HAVE_LIBLIFTOFF_0_5 1

#define HAVE_LINUX_SYNC_FILE 1

#define HAVE_XCB_ERRORS 0

#define ICONDIR "/usr/local/share/icons"
{XWAYLAND}
"""

XWAYLAND_CONFIG = """
#define XWAYLAND_PATH "/usr/bin/Xwayland"

#define HAVE_XWAYLAND_LISTENFD 0

#define HAVE_XWAYLAND_NO_TOUCH_POINTER_EMULATION 0

#define HAVE_XWAYLAND_FORCE_XRANDR_EMULATION 0

#define HAVE_XWAYLAND_TERMINATE_DELAY 0
"""

WLR_VERSION_H = """\
#ifndef WLR_VERSION_H
#define WLR_VERSION_H

#define WLR_VERSION_STR "0.21.0-dev"

#define WLR_VERSION_MAJOR 0
#define WLR_VERSION_MINOR 21
#define WLR_VERSION_MICRO 0

#define WLR_VERSION_NUM ((WLR_VERSION_MAJOR << 16) | (WLR_VERSION_MINOR << 8) | WLR_VERSION_MICRO)

/* For runtime version detection */
int wlr_version_get_major(void);
int wlr_version_get_minor(void);
int wlr_version_get_micro(void);

#endif
"""

# --------------------------------------------------------------------------
# config.mk -> feature flags
# --------------------------------------------------------------------------

def config_mk_flags():
    """Every -D option configured in config.mk (XWAYLAND, ...)."""
    try:
        text = open(CONFIG_MK, encoding='utf-8').read()
    except OSError:
        return set()
    # skip commented-out assignments
    flags = set()
    for line in text.splitlines():
        s = line.strip()
        if s.startswith('#'):
            continue
        flags.update(re.findall(r'-D([A-Za-z_]\w*)', line))
    return flags


def pkg_config(*args):
    try:
        out = subprocess.run(['pkg-config'] + list(args),
                             capture_output=True, text=True)
        if out.returncode == 0:
            return out.stdout.strip()
    except OSError:
        pass
    return None


# --------------------------------------------------------------------------
# generated support code (no compilation involved)
# --------------------------------------------------------------------------

def _fresh(out, *deps):
    if not os.path.exists(out):
        return False
    return all(os.path.getmtime(out) >= os.path.getmtime(d) for d in deps)


def _run(argv, stdin=None, stdout=None):
    res = subprocess.run(argv, input=stdin, stdout=stdout,
                         stderr=subprocess.PIPE, text=True)
    if res.returncode != 0:
        sys.exit('error: %s\n%s' % (' '.join(argv), res.stderr))


def gen_config_headers(flags):
    os.makedirs(os.path.join(GEN, 'include', 'wlr'), exist_ok=True)
    wlr_config = WLR_CONFIG_H
    for placeholder, key in (('{XWAYLAND}', 'XWAYLAND'),
                             ('{X11}', 'X11_BACKEND'),
                             ('{VULKAN}', 'VULKAN')):
        wlr_config = wlr_config.replace(
            placeholder, '1' if key in flags else '0')
    xwayland = 'XWAYLAND' in flags
    files = [
        (os.path.join(GEN, 'include', 'wlr', 'config.h'), wlr_config),
        (os.path.join(GEN, 'include', 'wlr', 'version.h'), WLR_VERSION_H),
        (os.path.join(GEN, 'include', 'config.h'),
         INTERNAL_CONFIG_H.replace('{XWAYLAND}',
                                   XWAYLAND_CONFIG if xwayland else '')),
    ]
    for path, text in files:
        old = None
        if os.path.exists(path):
            old = open(path, encoding='utf-8').read()
        if old != text:
            with open(path, 'w', encoding='utf-8') as fh:
                fh.write(text)


def protocol_table():
    """name -> xml path, mirroring wlroots/protocol/meson.build."""
    meson = open(os.path.join(WLR, 'protocol', 'meson.build'),
                 encoding='utf-8').read()
    block = re.search(r'protocols\s*=\s*\{(.*?)\n\}', meson, re.S)
    if not block:
        sys.exit('error: cannot read the protocol table from wlroots')
    wl_protocol_dir = pkg_config('--variable=pkgdatadir', 'wayland-protocols')
    if not wl_protocol_dir:
        sys.exit('error: pkg-config cannot find wayland-protocols')
    table = {}
    for m in re.finditer(r"'([\w-]+)':\s*(wl_protocol_dir\s*/\s*)?'([^']+)'",
                         block.group(1)):
        name, from_wlp, path = m.groups()
        table[name] = (os.path.join(wl_protocol_dir, path) if from_wlp
                       else os.path.join(WLR, 'protocol', path))
    return table


def gen_protocols():
    """wayland-scanner: private code + server/client headers for each protocol."""
    scanner = shutil.which('wayland-scanner') or pkg_config(
        '--variable=wayland_scanner', 'wayland-scanner')
    if not scanner:
        sys.exit('error: wayland-scanner not found')
    outdir = os.path.join(GEN, 'protocol')
    os.makedirs(outdir, exist_ok=True)
    outs = []
    for name, xml in sorted(protocol_table().items()):
        if not os.path.exists(xml):
            sys.exit('error: protocol xml not found: %s' % xml)
        base = os.path.splitext(os.path.basename(xml))[0]
        jobs = [('private-code', '-protocol.c'),
                ('server-header', '-protocol.h'),
                ('client-header', '-client-protocol.h')]
        for mode, suffix in jobs:
            out = os.path.join(outdir, base + suffix)
            if not _fresh(out, xml):
                _run([scanner, mode, xml, out])
            outs.append(out)
    return outs


def gen_shaders():
    """Embed the GLSL sources as C headers (wlroots' embed.sh)."""
    embed = os.path.join(WLR, 'render', 'gles2', 'shaders', 'embed.sh')
    outdir = os.path.join(GEN, 'render', 'gles2', 'shaders')
    os.makedirs(outdir, exist_ok=True)
    outs = []
    for name in SHADERS:
        src = os.path.join(WLR, 'render', 'gles2', 'shaders', name)
        var = re.sub(r'[^A-Za-z0-9_]', '_', name) + '_src'
        out = os.path.join(outdir, var + '.h')
        if not _fresh(out, src, embed):
            with open(src, encoding='utf-8') as fh:
                data = fh.read()
            with open(out, 'w', encoding='utf-8') as fh:
                _run(['/bin/sh', '-eu', embed, var], stdin=data, stdout=fh)
        outs.append(out)
    return outs


def _glslang():
    """(path, quiet) for the glslang frontend, mirroring wlroots' meson rule."""
    exe = shutil.which('glslang')
    if exe is None:
        return None, False
    try:
        res = subprocess.run([exe, '--version'], capture_output=True, text=True)
    except OSError:
        return None, False
    m = re.search(r'(\d+)\.(\d+)', res.stdout or '')
    ver = (int(m.group(1)), int(m.group(2))) if m else (0, 0)
    return exe, ver >= (11, 0)


def gen_vulkan_shaders():
    """SPIR-V shaders compiled to C headers (wlroots' shaders/meson.build)."""
    exe, quiet = _glslang()
    if exe is None:
        sys.exit('error: VULKAN is enabled but glslang was not found\n'
                 '       (install glslang-tools - it compiles the SPIR-V\n'
                 '       shaders while amalgamating)')
    outdir = os.path.join(GEN, 'render', 'vulkan', 'shaders')
    os.makedirs(outdir, exist_ok=True)
    outs = []
    for name in VULKAN_SHADERS:
        src = os.path.join(WLR, 'render', 'vulkan', 'shaders', name)
        out = os.path.join(outdir, name + '.h')
        var = re.sub(r'[^A-Za-z0-9_]', '_', name) + '_data'
        args = [exe, '-V', src, '-o', out, '--vn', var]
        if quiet:
            args.append('--quiet')
        if not _fresh(out, src):
            _run(args)
        outs.append(out)
    return outs


def gen_pnpids():
    """PCI vendor id table from hwdata's pnp.ids (wlroots' gen_pnpids.sh)."""
    hwdata = pkg_config('--variable=pkgdatadir', 'hwdata')
    pnp_ids = os.path.join(hwdata, 'pnp.ids') if hwdata else None
    if not pnp_ids or not os.path.exists(pnp_ids):
        sys.exit('error: hwdata/pnp.ids not found (needed for the DRM backend)')
    script = os.path.join(WLR, 'backend', 'drm', 'gen_pnpids.sh')
    outdir = os.path.join(GEN, 'backend', 'drm')
    os.makedirs(outdir, exist_ok=True)
    out = os.path.join(outdir, 'pnpids.c')
    if not _fresh(out, pnp_ids, script):
        with open(pnp_ids, encoding='utf-8') as fh:
            data = fh.read()
        with open(out, 'w', encoding='utf-8') as fh:
            _run(['/bin/sh', '-eu', script], stdin=data, stdout=fh)
    return [out]


# --------------------------------------------------------------------------
# include handling
# --------------------------------------------------------------------------

def include_dirs():
    # generated support files first: e.g. wlroots' #include "config.h" must
    # find build/include/config.h, not jtl's own config.h.
    return [os.path.join(GEN, 'include'),
            os.path.join(GEN, 'protocol'),
            os.path.join(GEN, 'render', 'gles2', 'shaders'),
            os.path.join(GEN, 'render', 'vulkan', 'shaders'),
            GEN,
            os.path.join(WLR, 'include'),
            WLR]


def inlinable(path):
    return (path.startswith(WLR + os.sep) or path.startswith(GEN + os.sep))


def resolve(inc, curfile, quoted, dirs):
    cands = []
    if quoted:
        cands.append(os.path.dirname(curfile))
    cands.extend(dirs)
    for d in cands:
        p = os.path.normpath(os.path.join(d, inc))
        if os.path.exists(p):
            return p
    return None


INC_RE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]')
COND_RE = re.compile(r'^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)')
HAS_RE = re.compile(r'^\s*(!?)\s*(WLR_HAS_[A-Z0-9_]+)\s*(?:/[/*].*)?$')
FEAT_RE = re.compile(
    r'^\s*#\s*(?:define\s+_(?:GNU_SOURCE|DEFAULT_SOURCE|XOPEN_SOURCE|BSD_SOURCE|SVID_SOURCE|POSIX_C_SOURCE)\b'
    r'|undef\s+_(?:POSIX_C_SOURCE|GNU_SOURCE|DEFAULT_SOURCE|XOPEN_SOURCE|BSD_SOURCE|SVID_SOURCE)\b)')
_features = None


def build_features():
    """WLR_HAS_* values from the generated wlr/config.h (cached)."""
    global _features
    if _features is None:
        _features = {}
        try:
            with open(os.path.join(GEN, 'include', 'wlr', 'config.h')) as fh:
                for line in fh:
                    m = re.match(r'\s*#\s*define\s+(WLR_HAS_\w+)\s+(\d+)', line)
                    if m:
                        _features[m.group(1)] = int(m.group(2))
        except OSError:
            pass
    return _features


def iter_includes(text):
    """Yield (quote, name) for #include lines that can actually be compiled.

    Includes inside a block guarded by a disabled `#if WLR_HAS_FOO` (or an
    enabled `#if !WLR_HAS_FOO`) are skipped, so optional backends that are
    turned off do not drag their headers (and system dependencies) in.  Any
    other conditional is treated as active.
    """
    feats = build_features()
    stack = []  # one bool per open #if: is the current branch live?
    for line in text.splitlines():
        c = COND_RE.match(line)
        if c:
            kw, rest = c.group(1), c.group(2)
            if kw in ('if', 'ifdef', 'ifndef'):
                active = True
                h = HAS_RE.match(rest) if kw == 'if' else None
                if h and h.group(2) in feats:
                    val = bool(feats[h.group(2)])
                    active = (not val) if h.group(1) else val
                stack.append(active)
            elif kw in ('elif', 'else') and stack:
                # An #else of a known-false branch is live, otherwise assume live.
                stack[-1] = True
            elif kw == 'endif' and stack:
                stack.pop()
            continue
        if not all(stack):
            continue
        m = INC_RE.match(line)
        if m:
            yield m.group(1), m.group(2)


def collect_headers(starts, dirs):
    """Topologically ordered inlinable headers (dependencies first)."""
    order = []
    state = {}

    def visit(path):
        st = state.get(path)
        if st is not None:
            return
        state[path] = 1
        try:
            text = open(path, encoding='utf-8', errors='replace').read()
        except OSError:
            text = ''
        for q, inc in iter_includes(text):
            r = resolve(inc, path, q == '"', dirs)
            if r and inlinable(r):
                visit(r)
        state[path] = 2
        if inlinable(path) and path.endswith('.h'):
            order.append(path)

    for s in starts:
        visit(s)
    return order


def strip_includes(text, curfile, dirs, macro_seen=None):
    out = []
    for line in text.splitlines(keepends=True):
        m = INC_RE.match(line)
        if m:
            r = resolve(m.group(2), curfile, m.group(1) == '"', dirs)
            if r and inlinable(r):
                continue
        if line.strip() == '#pragma once':
            continue
        if FEAT_RE.match(line):
            continue
        if macro_seen is not None:
            md = re.match(r'^\s*#\s*define\s+([A-Za-z_]\w*)', line)
            if md:
                name = md.group(1)
                if name in macro_seen and not name.startswith('_'):
                    out.append('#undef %s\n' % name)
                macro_seen.add(name)
        out.append(line)
    return ''.join(out)


def sanitize(rel):
    return re.sub(r'[^A-Za-z0-9_]', '_', rel)


def relabel(path):
    if path.startswith(WLR + os.sep):
        return os.path.relpath(path, WLR)
    return os.path.relpath(path, ROOT)


# --------------------------------------------------------------------------
# lexical renaming (per-source statics and clashing tags)
# --------------------------------------------------------------------------

def _is_ident(w):
    return bool(w) and (w[0].isalpha() or w[0] == '_')


def _walk(text):
    """Yield (kind, start, end, token), skipping comments and literals.

    kind is 'id', 'punct' or 'pp' (an #include line, treated as opaque).
    """
    i = 0
    n = len(text)
    line_start = True
    while i < n:
        c = text[i]
        if line_start and c == '#':
            j = text.find('\n', i)
            j = n if j < 0 else j
            if re.match(r'\s*#\s*include\b', text[i:j]):
                yield ('pp', i, j, text[i:j])
                i = j
                continue
        if c == '\n':
            yield ('punct', i, i + 1, c)
            i += 1
            line_start = True
            continue
        line_start = False
        if c.isspace():
            i += 1
            continue
        if c == '/' and i + 1 < n and text[i + 1] == '/':
            j = text.find('\n', i)
            j = n if j < 0 else j
            i = j
            continue
        if c == '/' and i + 1 < n and text[i + 1] == '*':
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            i = j
            continue
        if c == '"' or c == "'":
            q = c
            j = i + 1
            while j < n:
                if text[j] == '\\':
                    j += 2
                    continue
                if text[j] == q:
                    j += 1
                    break
                if text[j] == '\n':
                    break
                j += 1
            i = j
            continue
        if c.isalpha() or c == '_':
            j = i + 1
            while j < n and (text[j].isalnum() or text[j] == '_'):
                j += 1
            yield ('id', i, j, text[i:j])
            i = j
            continue
        if c == '-' and i + 1 < n and text[i + 1] == '>':
            yield ('punct', i, i + 2, '->')
            i += 2
            continue
        yield ('punct', i, i + 1, c)
        i += 1


def _classify(text):
    prev = None
    prev2 = None
    braces = []
    parens = []
    info = []
    for kind, a, b, tok in _walk(text):
        if kind == 'pp':
            prev = prev2 = None
            info.append((a, b, tok, {}))
            continue
        if kind == 'id':
            member = bool(parens and parens[-1]['skip']
                          and parens[-1]['commas'] + 1 == parens[-1]['skip'])
            ctx = {
                'in_type': bool(braces) and braces[-1] == 'T',
                'member': member,
                'after_tag_kw': prev in TAG_KW,
                'after_dot': prev in ('.', '->'),
            }
            info.append((a, b, tok, ctx))
            prev2 = prev
            prev = tok
            continue
        if tok == '{':
            is_type = (prev in TAG_KW) or (_is_ident(prev) and prev2 in TAG_KW)
            braces.append('T' if is_type else 'O')
        elif tok == '}':
            if braces:
                braces.pop()
        elif tok == '(':
            fname = tok if False else (prev if prev in MEMBER_CALLS else None)
            parens.append({'fname': fname, 'skip': MEMBER_CALLS.get(fname, 0),
                           'commas': 0})
        elif tok == ')':
            if parens:
                parens.pop()
        elif tok == ',':
            if parens:
                parens[-1]['commas'] += 1
        if tok != '\n':
            prev2 = prev
            prev = tok
        info.append((a, b, tok, {}))
    return info


MEMBER_CALLS = {'wl_container_of': 3, 'container_of': 3, 'offsetof': 2}
TAG_KW = ('struct', 'union', 'enum')


def lex_rename(text, names, prefix, tag_names=()):
    names = set(names)
    tag_names = set(tag_names)
    info = _classify(text)
    out = []
    last = 0
    for a, b, tok, ctx in info:
        out.append(text[last:a])
        if ctx.get('in_type'):
            out.append(tok)
        elif (tok in names and not ctx.get('member')
              and not ctx.get('after_dot') and not ctx.get('after_tag_kw')):
            out.append(prefix + tok)
        elif tok in tag_names and ctx.get('after_tag_kw'):
            out.append(prefix + tok)
        else:
            out.append(tok)
        last = b
    out.append(text[last:])
    return ''.join(out)


# --------------------------------------------------------------------------
# pruning: keep only the wlroots sources jtl.c can reach
# --------------------------------------------------------------------------

IDS_RE = re.compile(r'[A-Za-z_]\w*')
# file-scope, non-static function definitions (the name may sit on its own line)
FN_RE = re.compile(r'^(?![A-Za-z_ \t\*]*\b(?:static|extern)\b)'
                   r'[A-Za-z0-9_ \t\*]*?\b([A-Za-z_]\w*)\s*\([^;{]*\)\s*\{',
                   re.M)
# file-scope, non-static object definitions
VAR_RE = re.compile(r'^(?![A-Za-z_ \t\*]*\b(?:static|extern)\b)'
                    r'[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w*)\s*'
                    r'(?:\[[^\]\n]*\]\s*)?=(?!=)', re.M)


def _read(path):
    return open(path, encoding='utf-8', errors='replace').read()


def build_units(paths):
    """defs: symbol -> files defining it.  units: file -> every identifier."""
    defs = {}
    units = {}
    for p in paths:
        text = scan.strip_comments_strings(_read(p))
        ids = set(IDS_RE.findall(text))
        units[p] = ids
        for m in FN_RE.finditer(text):
            name = m.group(1)
            if name not in scan.KEYWORDS:
                defs.setdefault(name, set()).add(p)
        for m in VAR_RE.finditer(text):
            name = m.group(1)
            if name not in scan.KEYWORDS:
                defs.setdefault(name, set()).add(p)
    return defs, units


def header_refs(text):
    """Identifiers used by a header's static inline bodies and macro bodies.

    Only instantiated inline functions create link dependencies, so this looks
    at static function/variable initializers and #define bodies - plain
    declarations never pull in a source file.
    """
    s = scan.strip_comments_strings(text)
    ids = set()
    for m in re.finditer(r'\bstatic\b', s):
        i = m.end()
        j = s.find('{', i)
        k = s.find(';', i)
        if j < 0 or (0 <= k < j):
            continue
        depth = 0
        p = j
        while p < len(s):
            if s[p] == '{':
                depth += 1
            elif s[p] == '}':
                depth -= 1
                if depth == 0:
                    break
            p += 1
        ids.update(IDS_RE.findall(s[i:p]))
    for m in re.finditer(r'^[ \t]*#[ \t]*define[ \t]+[A-Za-z_]\w*.*(?:\\\n.*)*',
                         s, re.M):
        ids.update(IDS_RE.findall(m.group(0)))
    return ids


def jtl_seed_ids(flags, dirs):
    """Identifiers jtl.c refers to, preprocessed with config.mk's -D options.

    The #include "wlroots.c" line is dropped first: we want the needs of the
    compositor on its own, not of everything it pulls in.
    """
    text = re.sub(r'^[ \t]*#[ \t]*include[ \t]+"wlroots\.c"[ \t]*\n', '',
                  _read(JTL), flags=re.M)
    seed = os.path.join(ROOT, '.jtl-seed.c')
    with open(seed, 'w', encoding='utf-8') as fh:
        fh.write(text)
    argv = [os.environ.get('CC', 'cc'), '-E', '-I', ROOT, seed]
    for f in sorted(flags):
        argv.insert(3, '-D' + f)
    res = subprocess.run(argv, capture_output=True, text=True)
    os.unlink(seed)
    if res.returncode != 0:
        print('warning: could not preprocess jtl.c for pruning:\n%s'
              % res.stderr.splitlines()[0] if res.stderr else '',
              file=sys.stderr)
        print('warning: falling back to unpreprocessed identifiers',
              file=sys.stderr)
        text = scan.strip_comments_strings(_read(JTL))
    else:
        text = res.stdout
    return set(IDS_RE.findall(text))


def closure(seeds, units, defs):
    """Files (from units) reachable from seeds through defs."""
    need = set(seeds)
    files = set()
    work = list(need)
    while work:
        sym = work.pop()
        for f in defs.get(sym, ()):
            if f in files:
                continue
            files.add(f)
            for s in units[f]:
                if s not in need:
                    need.add(s)
                    work.append(s)
    return files


def prune(candidates, gen_candidates, dirs, flags):
    """Iteratively drop sources and headers jtl.c cannot reach."""
    defs, units = build_units(candidates + gen_candidates)
    seeds = jtl_seed_ids(flags, dirs)
    kept = list(candidates)
    kept_gen = list(gen_candidates)
    for _ in range(12):
        starts = kept + kept_gen + [JTL]
        order = collect_headers(starts, dirs)
        eff = set(seeds)
        for h in order:
            eff |= header_refs(_read(h))
        reach = closure(eff, units, defs)
        new = [p for p in candidates if p in reach]
        new_gen = [p for p in gen_candidates if p in reach]
        if set(new) == set(kept) and set(new_gen) == set(kept_gen):
            break
        kept, kept_gen = new, new_gen
    return kept, kept_gen


# --------------------------------------------------------------------------
# main
# --------------------------------------------------------------------------

def main():
    flags = config_mk_flags()
    prune_enabled = '--no-prune' not in sys.argv[1:]

    if not os.path.isdir(WLR):
        if os.path.exists(OUT):
            # wlroots/ is already gone and wlroots.c exists: nothing to do.
            print('wlroots/ not present; keeping the existing wlroots.c')
            os.utime(OUT)
            return
        sys.exit('error: wlroots/ not found and no wlroots.c to fall back on')
    if not os.path.exists(JTL):
        sys.exit('error: %s not found' % JTL)

    # 1. generated support code (wayland-scanner + two sh scripts, no compile)
    if 'VULKAN' in flags:
        if pkg_config('--exists', 'vulkan') is None:
            sys.exit('error: VULKAN is enabled but pkg-config cannot find vulkan\n'
                     '       (install vulkan-loader and the vulkan headers)')
        if shutil.which('glslang') is None:
            sys.exit('error: VULKAN is enabled but glslang is not installed\n'
                     '       (install glslang-tools - it compiles the SPIR-V\n'
                     '       shaders while amalgamating)')
    gen_config_headers(flags)
    gen_protocols()
    gen_shaders()
    gen_pnpids()
    if 'VULKAN' in flags:
        gen_vulkan_shaders()

    dirs = include_dirs()

    # 2. the candidate source set for this config.mk
    rel_sources = list(SOURCES)
    for flag, srcs in (('XWAYLAND', XWAYLAND_SOURCES),
                       ('X11_BACKEND', X11_SOURCES),
                       ('VULKAN', VULKAN_SOURCES)):
        if flag in flags:
            rel_sources += srcs
    candidates = [os.path.join(WLR, r) for r in rel_sources]
    for p in candidates:
        if not os.path.exists(p):
            sys.exit('error: missing wlroots source %s' % p)
    gen_c = [os.path.join(GEN, 'backend', 'drm', 'pnpids.c')]
    gen_c += [os.path.join(GEN, 'protocol', f)
              for f in sorted(os.listdir(os.path.join(GEN, 'protocol')))
              if f.endswith('-protocol.c')]
    missing = [p for p in gen_c if not os.path.exists(p)]
    if missing:
        sys.exit('error: generated code missing: %s' % ', '.join(missing))

    # 3. prune what jtl.c cannot reach
    total_src, total_gen = len(candidates), len(gen_c)
    if prune_enabled:
        candidates, gen_c = prune(candidates, gen_c, dirs, flags)
    pruned = (total_src + total_gen) - (len(candidates) + len(gen_c))

    # 4. headers, in dependency order
    order = collect_headers(candidates + gen_c + [JTL], dirs)

    # 5. per-file statics and clashing tags
    stdb = {}
    tags_db = {}
    for p in gen_c + candidates:
        st, tg = scan.scan(p)
        stdb[p] = st
        tags_db[p] = tg

    header_tags = set()
    for h in order:
        try:
            _st, tg = scan.scan(h)
        except Exception:
            tg = set()
        header_tags |= tg

    emitted = gen_c + candidates
    tags_by_file = {}
    for p in emitted:
        for t in tags_db.get(p, ()):
            tags_by_file.setdefault(t, []).append(p)
    tag_renames = {}
    for t, fs in tags_by_file.items():
        if t in header_tags:
            for f in fs:
                tag_renames.setdefault(f, set()).add(t)
        elif len(fs) > 1:
            for f in fs[1:]:
                tag_renames.setdefault(f, set()).add(t)

    little = (sys.byteorder == 'little')

    parts = []

    def emit(s):
        parts.append(s)

    feat = ' '.join(sorted(flags)) or '(none)'
    emit('/*\n'
         ' * wlroots.c - all of wlroots 0.21 plus its generated protocol,\n'
         ' * shader and config code, inlined into one file.\n'
         ' *\n'
         ' * GENERATED by amalgamate.py from the wlroots/ tree - do not edit.\n'
         ' * jtl.c includes this file; regenerate with `make amalgamate`\n'
         ' * (only possible while wlroots/ still exists).\n'
         ' *\n'
         ' * features from config.mk: %s\n'
         ' */\n' % feat)
    emit('#ifndef _GNU_SOURCE\n#define _GNU_SOURCE 1\n#endif\n')
    emit('#ifndef _DEFAULT_SOURCE\n#define _DEFAULT_SOURCE 1\n#endif\n')
    emit('#ifndef _POSIX_C_SOURCE\n#define _POSIX_C_SOURCE 200809L\n#endif\n')
    emit('#ifndef _FILE_OFFSET_BITS\n#define _FILE_OFFSET_BITS 64\n#endif\n')
    emit('#ifndef WLR_USE_UNSTABLE\n#define WLR_USE_UNSTABLE 1\n#endif\n')
    emit('#ifndef WLR_PRIVATE\n#define WLR_PRIVATE\n#endif\n')
    emit('#ifndef WLR_LITTLE_ENDIAN\n#define WLR_LITTLE_ENDIAN %d\n#endif\n'
         % (1 if little else 0))
    emit('#define WLR_BIG_ENDIAN %d\n' % (0 if little else 1))
    emit('\n')
    emit('/* The inlined wlroots code below is third-party and does not satisfy\n'
         ' * jtl\'s stricter warning set; silence those warnings for it. */\n')
    emit('#if defined(__GNUC__) && !defined(__clang__)\n')
    emit('#pragma GCC diagnostic push\n')
    for w in ('-Wformat', '-Wdeclaration-after-statement', '-Wunused-macros',
              '-Wfloat-conversion', '-Wshadow', '-Wpedantic',
              '-Wmissing-field-initializers', '-Wpragma-once-outside-header'):
        emit('#pragma GCC diagnostic ignored "%s"\n' % w)
    emit('#endif\n')
    emit('\n')

    macro_seen = set()

    for h in order:
        emit('\n/* ===== header: %s ===== */\n' % relabel(h))
        text = _read(h)
        emit(strip_includes(text, h, dirs, macro_seen=macro_seen))
        emit('\n')

    for p in gen_c:
        prefix = 'WLR_' + sanitize(relabel(p)) + '_'
        emit('\n/* ===== protocol: %s ===== */\n' % relabel(p))
        text = strip_includes(_read(p), p, dirs, macro_seen=macro_seen)
        emit(lex_rename(text, stdb[p], prefix, tag_renames.get(p, ())))
        emit('\n')

    for p in candidates:
        prefix = 'WLR_' + sanitize(relabel(p)) + '_'
        emit('\n/* ===== source: %s ===== */\n' % relabel(p))
        text = strip_includes(_read(p), p, dirs, macro_seen=macro_seen)
        emit(lex_rename(text, stdb[p], prefix, tag_renames.get(p, ())))
        emit('\n')

    emit('\n#if defined(__GNUC__) && !defined(__clang__)\n')
    emit('#pragma GCC diagnostic pop\n')
    emit('#endif\n')

    with open(OUT, 'w', encoding='utf-8') as fh:
        fh.write(''.join(parts))

    print('features: %s' % feat)
    print('headers: %d  protocol: %d  wlr: %d%s' % (
        len(order), len(gen_c), len(candidates),
        '  (pruned %d of %d unused files)' % (pruned, total_src + total_gen)
        if prune_enabled else ''))
    print('file-local statics: %d' % sum(len(v) for v in stdb.values()))
    print('wrote %s (%.1f MiB)' % (OUT, os.path.getsize(OUT) / (1024.0 * 1024)))


if __name__ == '__main__':
    main()
