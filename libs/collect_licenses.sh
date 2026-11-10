#!/usr/bin/env bash
# collect_licenses.sh <source dir> <out dir> <name>
# Copies the license / notice files of a source tree, of its meson subprojects (e.g. abseil) and of third-party code vendored
# inside it, to <out dir>/<name>/ so a binary package can carry them (BSD-3-Clause and Apache-2.0 require the text to travel with
# the binary). It FAILS (exit 1) when the main license or the license of a subproject cannot be found, so a changed source layout
# can never produce a package without the texts. The list of what was copied is written to <out dir>/<name>/INDEX.txt.
set -euo pipefail
SRC="$(cd "${1:?source dir}" && pwd)"; OUT="${2:?out dir}"; NAME="${3:?name}"
DEST="$OUT/$NAME"; mkdir -p "$DEST"; DEST="$(cd "$DEST" && pwd)"
PAT=( -iname 'LICENSE*' -o -iname 'LICENCE*' -o -iname 'COPYING*' -o -iname 'NOTICE*' -o -iname 'PATENTS*' -o -iname 'AUTHORS*' )

copy_top() {   # <dir> <prefix>  -> number of files copied
    local d="$1" p="$2" n=0 f
    while IFS= read -r f; do cp "$f" "$DEST/${p}$(basename "$f")"; n=$((n+1)); done \
        < <(find "$d" -maxdepth 1 -type f \( "${PAT[@]}" \) | sort)
    echo "$n"
}

n=$(copy_top "$SRC" "")
[ "$n" -gt 0 ] || { echo "[error] collect_licenses: no LICENSE/COPYING in $SRC"; exit 1; }

for s in "$SRC"/subprojects/*/; do
    [ -d "$s" ] || continue
    case "$(basename "$s")" in packagecache|packagefiles) continue;; esac      # meson's download cache / patch dirs, not sources
    n=$(copy_top "${s%/}" "$(basename "$s")-")
    [ "$n" -gt 0 ] || { echo "[error] collect_licenses: no LICENSE/COPYING in subproject $s"; exit 1; }
done

# third-party code vendored inside the tree (anything with its own license file below the top, outside subprojects / build dirs)
while IFS= read -r f; do
    rel="${f#"$SRC"/}"; cp "$f" "$DEST/$(echo "$rel" | tr '/' '_')"
done < <(find "$SRC" -mindepth 2 -maxdepth 8 -type f \( "${PAT[@]}" \) \
            -not -path "$SRC/subprojects/*" -not -path '*/build/*' -not -path '*/.git/*' | sort)

( cd "$DEST" && ls -1 | grep -v '^INDEX.txt$' > INDEX.txt; echo "collected from: $SRC" >> INDEX.txt )
echo "[licenses] $NAME: $(($(wc -l < "$DEST/INDEX.txt") - 1)) files"; cat "$DEST/INDEX.txt"
