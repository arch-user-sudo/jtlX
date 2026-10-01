#!/bin/bash
# Trim vendored source trees so only what the Makefile compiles remains.
# Removes git/CI/editor cruft, READMEs and license files from every dep.
#
# Usage:
#   ./trim-deps.sh           # trim all five vendored deps
#   ./trim-deps.sh pixman    # trim only the named deps
#
# Re-run after copying in a newer version of any vendored library.

set -e
cd "$(dirname "$0")"

DEPS=("$@")
[ "${#DEPS[@]}" -gt 0 ] || DEPS=(wlroots libliftoff libdisplay-info pixman libwayland)

trim() {
	local dir="$1"
	if [ ! -d "$dir" ]; then
		echo "skipping $dir (not present)"
		return
	fi
	echo "== trim: $dir =="

	# wlroots ships docs/tests/example code and release artifacts
	if [ "$dir" = wlroots ]; then
		echo "  removing docs, examples, tests, tinywl, subprojects, release.sh"
		rm -rf wlroots/docs wlroots/examples wlroots/test wlroots/tinywl \
			wlroots/subprojects wlroots/release.sh
	fi

	# libwayland ships docs/tests plus wayland-scanner and the client-side
	# libwayland-cursor/libwayland-egl helpers, none of which are built here
	# (protocol XMLs are scanned with the system wayland-scanner)
	if [ "$dir" = libwayland ]; then
		echo "  removing docs, tests, cursor, egl, wayland-scanner"
		rm -rf libwayland/doc libwayland/tests libwayland/cursor libwayland/egl \
			libwayland/release.sh libwayland/wayland-scanner.m4 \
			libwayland/wayland-scanner.mk libwayland/src/scanner.c \
			libwayland/src/embed.py libwayland/protocol/tests.xml \
			libwayland/protocol/generate-shm-formats.py \
			libwayland/protocol/wayland.dtd
	fi

	# git/CI/editor cruft + readmes, contribution guides and license files,
	# anywhere in the tree
	find "$dir" \( -name '.git*' -o -name '.gitlab*' -o -name '.editorconfig' \
		-o -name '.builds' -o -name '.mailmap' -o -name '.triage-policies.yml' \
		-o -iname 'readme*' -o -iname 'contributing*' -o -iname 'releasing*' \
		-o -iname 'license*' -o -iname 'copying*' \) \
		-prune -exec rm -rf {} + 2>/dev/null || true

	echo "  remaining: $(du -sh "$dir" | cut -f1)"
}

for dep in "${DEPS[@]}"; do
	trim "$dep"
done

echo "== sanity checks =="
check() {
	if [ -e "$1" ]; then
		echo "  ok: $1"
	else
		echo "  MISSING: $1"
		exit 1
	fi
}
check wlroots/backend/drm/libliftoff.c
check wlroots/meson.build
check libliftoff/include/libliftoff.h
check libdisplay-info/include/libdisplay-info/edid.h
check libdisplay-info/tool/gen-search-table.py
check pixman/pixman.h
check pixman/pixman-version.h.in
check pixman/pixman-region.c
check libwayland/protocol/wayland.xml
check libwayland/src/wayland-version.h.in
check libwayland/src/wayland-server.c
check libwayland/src/wayland-client.c
check libwayland/src/connection.c

echo "Done."