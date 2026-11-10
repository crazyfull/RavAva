#!/usr/bin/env bash
# collect_licenses.sh <source dir> <out dir> <name>
# Copies the license / notice files of a source tree (and of its meson subprojects, e.g. abseil) to <out dir>/<name>/ so that
# a binary package can carry them (BSD-3-Clause and Apache-2.0 require the text to travel with the binary).
set -euo pipefail
SRC="${1:?source dir}"; OUT="${2:?out dir}"; NAME="${3:?name}"
mkdir -p "$OUT/$NAME"
copy_from() {   # <dir> <prefix>
    local d="$1" p="$2" f
    for f in "$d"/LICENSE* "$d"/LICENCE* "$d"/COPYING* "$d"/NOTICE* "$d"/PATENTS* "$d"/AUTHORS*; do
        [ -f "$f" ] && cp "$f" "$OUT/$NAME/${p}$(basename "$f")"
    done
    return 0
}
copy_from "$SRC" ""
for s in "$SRC"/subprojects/*/; do
    [ -d "$s" ] && copy_from "${s%/}" "$(basename "$s")-"
done
# third-party code vendored inside the tree (pffft, rnnoise, ... keep their own license next to the code)
find "$SRC" -mindepth 2 -maxdepth 4 -type f \( -iname 'LICENSE*' -o -iname 'COPYING*' -o -iname 'NOTICE*' \) \
     -not -path "$SRC/subprojects/*" -not -path '*/build/*' 2>/dev/null | while read -r f; do
    rel="${f#"$SRC"/}"; cp "$f" "$OUT/$NAME/$(echo "$rel" | tr '/' '_')"
done
ls "$OUT/$NAME"
