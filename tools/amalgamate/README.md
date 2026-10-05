# jtl amalgamation tooling

`jtl.c` is a single self-contained source file: it contains the compositor's
hand-written code **plus** the wlroots headers, the generated Wayland protocol
code and the wlroots implementation files, all inlined.  There is no wlroots
library to link against.

The file you edit is `jtl.c.orig`.  `jtl.c` is generated and must not be edited
by hand.

## Regenerating

From the repository root:

```sh
make amalgamate      # rewrite jtl.c from jtl.c.orig + wlroots
make                 # rebuild ./jtl (regenerates jtl.c if jtl.c.orig is newer)
```

`make` also regenerates `jtl.c` automatically when `jtl.c.orig` changes.

## How it works

* `wlroots/` is a wlroots source checkout (read-only from the generator's point
  of view).  The generator runs `meson setup` in `wlroots/build` the first time
  and `ninja` to produce the generated `config.h`/`version.h`, the Wayland
  protocol `.c`/`.h` files and `compile_commands.json`.
* `amalgamate.py` reads `compile_commands.json` to learn the exact source list
  and include search path, then inlines every reachable wlroots header in
  dependency order, followed by the generated protocol sources, the wlroots
  sources, and finally `jtl.c.orig`.
* `scan.py` finds file-scope `static` declarations and `struct`/`union`/`enum`
  tags.  Because merging many translation units into one would otherwise create
  duplicate definitions, every source's file-local statics are renamed with a
  per-file prefix and clashing tags are renamed as needed.  Renaming is
  context-aware so struct members, `.`/`->` accesses and
  `container_of`/`offsetof` member arguments are left alone.
* The inlined third-party region is wrapped in `#pragma GCC diagnostic` pushes
  so only the compositor's own code is held to jtl's strict warning set.

The environment variable `WLR_BUILD` overrides the wlroots build directory
(default `wlroots/build`).  Pass `--no-build` to skip the `meson`/`ninja` step
and use an existing build directory as-is.
