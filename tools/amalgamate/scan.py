"""Minimal C scanner used by the jtl/wlroots amalgamation.

It only needs to answer two questions about a translation unit:

  * which identifiers are declared ``static`` at *file scope* (these must be
    renamed per-source so that merging every wlroots .c into one file does not
    create duplicate definitions), and
  * which ``struct``/``union``/``enum`` tags are *defined* at file scope (used
    to resolve tag name clashes).

The scanner deliberately ignores anything that is not at brace/paren/bracket
depth zero, so function bodies, parameter lists and array parameters such as
``float m[static 9]`` are never mistaken for file-scope declarations.
"""

import re

# Keywords, qualifiers and compiler extensions that can never be the name of a
# declarator.  Anything else that shows up before a ';' / '=' is a candidate.
KEYWORDS = set("""
auto break case char const continue default do double else enum extern float for goto if
inline int long register restrict return short signed sizeof static struct switch typedef
union unsigned void volatile while _Bool _Complex _Imaginary _Atomic _Noreturn
_Thread_local _Alignas _Alignof _Generic _Static_assert
__attribute __attribute__ __extension__ __inline __inline__ __restrict __restrict__
__const __const__ __volatile __volatile__ __signed __signed__ __typeof __typeof__ typeof
__asm __asm__ asm __auto_type __label__ __builtin_va_list __gnuc_va_list
""".split())

_TOK = re.compile(r'[A-Za-z_]\w*|0[xX][0-9a-fA-F]+|\d+|\S')
_TAGKW = ('struct', 'union', 'enum')


def strip_comments_strings(s):
    """Blank out comments and string/char literals, preserving offsets."""
    out = list(s)
    i = 0
    n = len(s)
    while i < n:
        c = s[i]
        if c == '/' and i + 1 < n and s[i + 1] == '/':
            j = s.find('\n', i)
            if j < 0:
                j = n
            for k in range(i, j):
                out[k] = ' '
            i = j
        elif c == '/' and i + 1 < n and s[i + 1] == '*':
            j = s.find('*/', i + 2)
            if j < 0:
                j = n
            else:
                j += 2
            for k in range(i, j):
                if s[k] != '\n':
                    out[k] = ' '
            i = j
        elif c == '"' or c == "'":
            q = c
            j = i + 1
            out[i] = ' '
            while j < n:
                if s[j] == '\\':
                    out[j] = ' '
                    if j + 1 < n:
                        out[j + 1] = ' '
                    j += 2
                    continue
                if s[j] == q:
                    out[j] = ' '
                    j += 1
                    break
                if s[j] == '\n':
                    break
                out[j] = ' '
                j += 1
            i = j
        else:
            i += 1
    return ''.join(out)


def toks(s):
    for m in _TOK.finditer(s):
        yield m.group(0), m.start()


def depth_at(s):
    d = [0] * len(s)
    cur = 0
    for i, ch in enumerate(s):
        d[i] = cur
        if ch == '{':
            cur += 1
        elif ch == '}':
            cur -= 1
    return d


def toplevel_mask(s):
    """True at offsets where brace, paren and bracket depth are all zero."""
    b = p = q = 0
    m = [False] * len(s)
    for i, ch in enumerate(s):
        m[i] = (b == 0 and p == 0 and q == 0)
        if ch == '{':
            b += 1
        elif ch == '}':
            b -= 1
        elif ch == '(':
            p += 1
        elif ch == ')':
            p -= 1
        elif ch == '[':
            q += 1
        elif ch == ']':
            q -= 1
    return m


def is_id(t):
    return bool(re.match(r'[A-Za-z_]\w*$', t))


_ATTRS = ('__attribute__', '__attribute', '__declspec', '__asm__', '__asm', 'asm')


def _skip_parens(T, j):
    """Given T[j] == '(', return the index just past its matching ')'."""
    depth = 0
    n = len(T)
    while j < n:
        if T[j][0] == '(':
            depth += 1
        elif T[j][0] == ')':
            depth -= 1
            if depth == 0:
                return j + 1
        j += 1
    return j


def _parse_static(T, i):
    """Parse the declaration that starts at the 'static' token T[i].

    Returns (declared_names, next_index).
    """
    names = []
    j = i + 1
    paren = 0
    brack = 0
    last = None
    in_params = False
    n = len(T)
    while j < n:
        tt = T[j][0]
        if T[j][2] > 0:                       # fell into a function body
            break
        if paren == 0 and brack == 0:
            if tt == ';':
                if last:
                    names.append(last)
                j += 1
                break
            if tt == '{':
                # A tag definition (struct/union/enum NAME { ... }) can be a
                # declaration specifier; skip its body and keep looking.
                if j >= 2 and is_id(T[j - 1][0]) and T[j - 2][0] in _TAGKW:
                    depth = 0
                    while j < n:
                        if T[j][0] == '{':
                            depth += 1
                        elif T[j][0] == '}':
                            depth -= 1
                            if depth == 0:
                                j += 1
                                break
                        j += 1
                    continue
                break                          # function body
        if tt in _ATTRS:
            k = j + 1
            while k < n and T[k][0] != '(':
                k += 1
            if k < n:
                j = _skip_parens(T, k)
                continue
        if tt == '(':
            nxt = T[j + 1][0] if j + 1 < n else None
            if paren == 0 and brack == 0 and nxt != '*' and last and last not in KEYWORDS:
                names.append(last)             # function name
                in_params = True
                last = None
            paren += 1
        elif tt == ')':
            paren = max(0, paren - 1)
            if paren == 0:
                in_params = False
        elif tt == '[':
            brack += 1
        elif tt == ']':
            brack = max(0, brack - 1)
        elif tt == '=' and paren == 0 and brack == 0:
            if last:
                names.append(last)
            last = None
            depth = 0
            j += 1
            while j < n:                       # skip the initializer
                x = T[j][0]
                if x in '([{':
                    depth += 1
                elif x in ')]}':
                    if x == '}' and depth == 0:
                        break
                    depth -= 1
                elif depth == 0 and x in (',', ';'):
                    break
                j += 1
            if j < n and T[j][0] == ',':
                j += 1
                last = None
                continue
            break
        elif tt == ',' and paren == 0 and brack == 0:
            if last:
                names.append(last)
            last = None
        elif (is_id(tt) and tt not in KEYWORDS and brack == 0 and not in_params):
            prev = T[j - 1][0] if j > 0 else None
            if prev not in _TAGKW:             # don't treat struct tags as names
                last = tt
        j += 1
    return names, j


def scan(path):
    """Return (file_scope_statics, file_scope_tags) for a C file or header."""
    raw = open(path, encoding='utf-8', errors='replace').read()
    s = strip_comments_strings(raw)
    d = depth_at(s)
    mask = toplevel_mask(s)
    T = [(t, p, d[p]) for (t, p) in toks(s) if t.strip() != '']
    statics = set()
    tags = set()
    i = 0
    while i < len(T):
        t, p, dep = T[i]
        if dep != 0 or not mask[p]:
            i += 1
            continue
        if (t in _TAGKW and i + 2 < len(T) and is_id(T[i + 1][0])
                and T[i + 1][2] == 0 and T[i + 2][2] == 0 and T[i + 2][0] == '{'):
            tags.add(T[i + 1][0])
        elif t == 'static':
            names, j = _parse_static(T, i)
            statics.update(names)
            i = j
            continue
        i += 1
    return statics, tags


if __name__ == '__main__':
    import sys
    for p in sys.argv[1:]:
        st, tg = scan(p)
        print(p)
        print('  statics  =', sorted(st))
        print('  tags     =', sorted(tg))
