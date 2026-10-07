#!/usr/bin/env python3
"""Amalgamate wlroots into a single self-contained jtl.c.

The compositor's hand-written source is ``jtl.c.orig``.  This script inlines
the wlroots headers, the generated Wayland protocol sources and the wlroots
implementation files ahead of it and writes the result to ``jtl.c``.

Merging many translation units into one requires a little care:

* file-scope ``static`` symbols from different sources would collide, so each
  source's statics are renamed with a per-file prefix (context-aware, so struct
  members, ``.``/``->`` accesses and ``container_of``/``offsetof`` member
  arguments are left alone);
* ``struct``/``union``/``enum`` tags defined in more than one place are renamed
  in all but the first definition (and in sources that clash with a header);
* feature-test macros and duplicate macro definitions are normalised;
* the third-party region is wrapped in diagnostic pragmas so that only the
  compositor's own code is checked against jtl's strict warning set.

The wlroots build directory (``wlroots/build`` by default, override with
``WLR_BUILD``) must already exist; use ``tools/amalgamate/build.sh`` or the
Makefile to configure and build it.
"""

import os
import re
import sys
import json
import shlex
import subprocess
from collections import defaultdict

sys.dont_write_bytecode = True
import scan  # noqa: E402  (local module, imported after the flag is set)

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
WLR = os.path.join(ROOT, 'wlroots')
BUILD = os.environ.get('WLR_BUILD', os.path.join(WLR, 'build'))
JTL_SRC = os.path.join(ROOT, 'jtl.c.orig')
OUT = os.path.join(ROOT, 'jtl.c')

# Locally generated protocol headers that the compositor includes directly;
# the wlroots build generates equivalent headers which get inlined instead.
PROJECT_PROTOS = {
    'xdg-shell-protocol.h',
    'cursor-shape-v1-protocol.h',
    'pointer-constraints-unstable-v1-protocol.h',
    'wlr-layer-shell-unstable-v1-protocol.h',
}

INC_RE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]')
FEAT_RE = re.compile(
    r'^\s*#\s*(?:define\s+_(?:GNU_SOURCE|DEFAULT_SOURCE|XOPEN_SOURCE|BSD_SOURCE|SVID_SOURCE|POSIX_C_SOURCE)\b'
    r'|undef\s+_(?:POSIX_C_SOURCE|GNU_SOURCE|DEFAULT_SOURCE|XOPEN_SOURCE|BSD_SOURCE|SVID_SOURCE)\b)')

MEMBER_CALLS = {'wl_container_of': 3, 'container_of': 3, 'offsetof': 2}
TAG_KW = ('struct', 'union', 'enum')


# --------------------------------------------------------------------------
# lexical renaming
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
            fname = prev if prev in MEMBER_CALLS else None
            parens.append({'fname': fname, 'skip': MEMBER_CALLS.get(fname, 0), 'commas': 0})
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
# include handling
# --------------------------------------------------------------------------

def make_include_dirs():
    dirs = []
    with open(os.path.join(BUILD, 'compile_commands.json')) as fh:
        cc = json.load(fh)
    for e in cc:
        toks = shlex.split(e['command'])
        for i, t in enumerate(toks):
            d = None
            if t == '-I' and i + 1 < len(toks):
                d = toks[i + 1]
            elif t.startswith('-I') and len(t) > 2:
                d = t[2:]
            elif t == '-isystem' and i + 1 < len(toks):
                d = toks[i + 1]
            elif t.startswith('-isystem') and len(t) > len('-isystem'):
                d = t[len('-isystem'):]
            if d is not None:
                p = os.path.normpath(os.path.join(BUILD, d))
                if p not in dirs:
                    dirs.append(p)
    return cc, dirs


def inlinable(path):
    return path.endswith('.h') and (path == WLR or path.startswith(WLR + os.sep))


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


COND_RE = re.compile(r'^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)')
HAS_RE = re.compile(r'^\s*(!?)\s*(WLR_HAS_[A-Z0-9_]+)\s*(?:/[/*].*)?$')
_features = None


def build_features():
    """WLR_HAS_* values from the generated wlr/config.h (cached)."""
    global _features
    if _features is None:
        _features = {}
        try:
            with open(os.path.join(BUILD, 'include', 'wlr', 'config.h')) as fh:
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
        if inlinable(path):
            order.append(path)

    for s in starts:
        visit(s)
    return order


def strip_includes(text, curfile, dirs, jtl=False, macro_seen=None):
    out = []
    for line in text.splitlines(keepends=True):
        m = INC_RE.match(line)
        if m:
            q, inc = m.group(1), m.group(2)
            r = resolve(inc, curfile, q == '"', dirs)
            if r and inlinable(r):
                continue
            if jtl and os.path.basename(inc) in PROJECT_PROTOS:
                continue
        if line.strip() == '#pragma once':
            continue
        if not jtl and FEAT_RE.match(line):
            continue
        if not jtl and macro_seen is not None:
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


# --------------------------------------------------------------------------
# main
# --------------------------------------------------------------------------

def ensure_build():
    if not os.path.isdir(WLR):
        sys.exit('error: wlroots source not found at %s' % WLR)
    if not os.path.exists(os.path.join(BUILD, 'build.ninja')):
        subprocess.check_call(['meson', 'setup', BUILD, WLR,
                               '-Dexamples=false', '-Dtests=false'])
    subprocess.check_call(['ninja', '-C', BUILD])


def main():
    if '--no-build' not in sys.argv[1:]:
        ensure_build()

    if not os.path.exists(JTL_SRC):
        sys.exit('error: %s not found' % JTL_SRC)

    cc, dirs = make_include_dirs()
    gen_srcs = []
    wlr_srcs = []
    for e in cc:
        p = os.path.normpath(os.path.join(BUILD, e['file']))
        if re.search(r'protocol/.*-protocol\.c$', e['file']):
            gen_srcs.append(p)
        else:
            wlr_srcs.append(p)
    gen_srcs.sort()
    wlr_srcs.sort()

    starts = wlr_srcs + gen_srcs + [JTL_SRC]
    order = collect_headers(starts, dirs)

    # per-file statics and tags
    stdb = {}
    tags_db = {}
    for p in wlr_srcs + gen_srcs:
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

    emitted = gen_srcs + wlr_srcs
    tags_by_file = defaultdict(list)
    for p in emitted:
        for t in tags_db.get(p, ()):
            tags_by_file[t].append(p)
    tag_renames = defaultdict(set)
    for t, fs in tags_by_file.items():
        if t in header_tags:
            for f in fs:
                tag_renames[f].add(t)
        elif len(fs) > 1:
            for f in fs[1:]:
                tag_renames[f].add(t)

    little = (sys.byteorder == 'little')

    parts = []

    def emit(s):
        parts.append(s)

    emit('/*\n'
         ' * jtl.c - self-contained amalgamation of the jtl compositor and the\n'
         ' * wlroots 0.21 library sources/headers, plus the generated Wayland\n'
         ' * protocol code.  This file is machine-generated: do not edit it by hand.\n'
         ' *\n'
         ' * The hand-written compositor code is at the end of this file (search for\n'
         ' * "===== jtl.c =====").  Edit jtl.c.orig and run `make amalgamate`.\n'
         ' */\n')
    emit('#ifndef _GNU_SOURCE\n#define _GNU_SOURCE 1\n#endif\n')
    emit('#ifndef _DEFAULT_SOURCE\n#define _DEFAULT_SOURCE 1\n#endif\n')
    emit('#ifndef _POSIX_C_SOURCE\n#define _POSIX_C_SOURCE 200809L\n#endif\n')
    emit('#ifndef _FILE_OFFSET_BITS\n#define _FILE_OFFSET_BITS 64\n#endif\n')
    emit('#ifndef WLR_USE_UNSTABLE\n#define WLR_USE_UNSTABLE 1\n#endif\n')
    emit('#ifndef WLR_PRIVATE\n#define WLR_PRIVATE\n#endif\n')
    emit('#ifndef WLR_LITTLE_ENDIAN\n#define WLR_LITTLE_ENDIAN %d\n#endif\n' % (1 if little else 0))
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
        rel = os.path.relpath(h, WLR)
        emit('\n/* ===== header: %s ===== */\n' % rel)
        text = open(h, encoding='utf-8', errors='replace').read()
        emit(strip_includes(text, h, dirs, macro_seen=macro_seen))
        emit('\n')

    for p in gen_srcs:
        rel = os.path.relpath(p, WLR)
        prefix = 'WLR_' + sanitize(rel) + '_'
        emit('\n/* ===== protocol: %s ===== */\n' % rel)
        text = open(p, encoding='utf-8', errors='replace').read()
        text = strip_includes(text, p, dirs, macro_seen=macro_seen)
        emit(lex_rename(text, stdb[p], prefix, tag_renames.get(p, ())))
        emit('\n')

    for p in wlr_srcs:
        rel = os.path.relpath(p, WLR)
        prefix = 'WLR_' + sanitize(rel) + '_'
        emit('\n/* ===== source: %s ===== */\n' % rel)
        text = open(p, encoding='utf-8', errors='replace').read()
        text = strip_includes(text, p, dirs, macro_seen=macro_seen)
        emit(lex_rename(text, stdb[p], prefix, tag_renames.get(p, ())))
        emit('\n')

    emit('\n#if defined(__GNUC__) && !defined(__clang__)\n')
    emit('#pragma GCC diagnostic pop\n')
    emit('#endif\n')
    emit('\n/* ===== jtl.c ===== */\n')
    jtl = open(JTL_SRC, encoding='utf-8', errors='replace').read()
    emit(strip_includes(jtl, JTL_SRC, dirs, jtl=True))
    emit('\n')

    with open(OUT, 'w', encoding='utf-8') as fh:
        fh.write(''.join(parts))

    dup = {t: [os.path.relpath(x, WLR) for x in fs]
           for t, fs in tags_by_file.items() if len(fs) > 1}
    print('headers: %d  protocol: %d  wlr: %d' % (len(order), len(gen_srcs), len(wlr_srcs)))
    print('file-local statics: %d' % sum(len(v) for v in stdb.values()))
    print('duplicate tags: %s' % (dup if dup else 'none'))
    print('wrote %s (%d bytes)' % (OUT, os.path.getsize(OUT)))


if __name__ == '__main__':
    main()
