#!/bin/bash

set -uo pipefail

REQUIRED_COMPONENTS="protobuf zlib zstd lz4 icu abseil-cpp libkmip quicklz boost"
MIN_COMPONENTS=10

ERRORS=0
err() { echo "validate-sbom: ERROR: $*" >&2; ERRORS=$((ERRORS + 1)); }
ok()  { echo "validate-sbom: OK: $*"; }

[ $# -gt 0 ] || { echo "usage: $0 ARTIFACT [ARTIFACT...]" >&2; exit 2; }

TMP=$(mktemp -d "${TMPDIR:-/tmp}/pxb-vsbom.XXXXXX")
trap 'rm -rf "$TMP"' EXIT

extract() {
    local art=$1 dest=$2
    mkdir -p "$dest"
    case "$art" in
        *.rpm)    (cd "$dest" && rpm2cpio "$art" | cpio -idmu --quiet) ;;
        *.deb)    dpkg-deb -x "$art" "$dest" ;;
        *.tar.gz) tar xzf "$art" -C "$dest" ;;
        *)        [ -d "$art" ] && cp -r "$art/." "$dest/" ;;
    esac
}

check_json() {
    local f=$1
    if command -v python3 >/dev/null 2>&1; then
        python3 -c "import json,sys; json.load(open(sys.argv[1]))" "$f" 2>/dev/null
        return $?
    fi
    grep -q '^}' "$f" 2>/dev/null
}

idx=0
for art in "$@"; do
    idx=$((idx + 1))
    [ -e "$art" ] || { err "artifact not found: $art"; continue; }
    name=$(basename "$art")
    dest="${TMP}/$(printf '%03d' "$idx")_$(echo "$name" | tr -c 'A-Za-z0-9._-' '_')"
    rm -rf "$dest"
    extract "$art" "$dest" || { err "$name: extraction failed"; continue; }

    mapfile -t sbom_dirs < <(find "$dest" -type d -name sbom 2>/dev/null)
    if [ ${#sbom_dirs[@]} -eq 0 ]; then
        err "$name: no sbom/ directory found"
        continue
    fi

    for d in "${sbom_dirs[@]}"; do
        rel=${d#"$dest"}
        errors_before=$ERRORS

        spdx=$(find "$d" -name '*.spdx.json' | head -1)
        cdx=$(find "$d" -name '*.cdx.json' | head -1)
        table=$(find "$d" -name '*.sbom.txt' | head -1)
        lic=$(find "$d" -name '*.licenses.txt' | head -1)

        for pair in "spdx.json:$spdx" "cdx.json:$cdx" "sbom.txt:$table" "licenses.txt:$lic"; do
            label=${pair%%:*}; path=${pair#*:}
            [ -n "$path" ] || err "$name:$rel: missing ${label}"
        done
        [ -n "$spdx" ] && [ -n "$cdx" ] || continue

        check_json "$spdx" || err "$name:$rel: $(basename "$spdx") is not valid JSON"
        check_json "$cdx"  || err "$name:$rel: $(basename "$cdx") is not valid JSON"

        count=$(grep -o '"versionInfo"' "$spdx" 2>/dev/null | wc -l | tr -d ' ')
        if [ "${count:-0}" -lt "$MIN_COMPONENTS" ]; then
            err "$name:$rel: only ${count} components in SPDX (expected >= ${MIN_COMPONENTS})"
        fi

        missing=""
        for c in $REQUIRED_COMPONENTS; do
            grep -q "\"name\": \"${c}\"" "$spdx" || missing="${missing} ${c}"
        done
        [ -n "$missing" ] && err "$name:$rel: SPDX missing expected components:${missing}"

        if [ -n "$lic" ]; then
            total=$(grep -c . "$lic" 2>/dev/null || true)
            noass=$(grep -c 'NOASSERTION' "$lic" 2>/dev/null || true)
            if [ "${total:-0}" -gt 0 ] && [ "${noass:-0}" -eq "${total:-0}" ]; then
                err "$name:$rel: every licence in $(basename "$lic") is NOASSERTION"
            fi
        fi

        [ $ERRORS -eq $errors_before ] && ok "$name:$rel: ${count} components, all checks passed"
    done
done

if [ $ERRORS -gt 0 ]; then
    echo "validate-sbom: FAILED with ${ERRORS} error(s)" >&2
    exit 1
fi
echo "validate-sbom: all artifacts passed"
