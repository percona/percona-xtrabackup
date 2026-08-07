#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT="."
REGISTRY="${SCRIPT_DIR}/components.txt"

KNOWN_UNCERTAIN=""

while [ $# -gt 0 ]; do
    case "$1" in
        --root)     ROOT=${2:?};     shift 2 ;;
        --registry) REGISTRY=${2:?}; shift 2 ;;
        *) echo "check-components: unknown argument: $1" >&2; exit 2 ;;
    esac
done

[ -f "$REGISTRY" ] || { echo "check-components: registry not found: $REGISTRY" >&2; exit 2; }
[ -d "$ROOT/extra" ] || { echo "check-components: no extra/ under root: $ROOT" >&2; exit 2; }

ERRORS=0
err()  { echo "check-components: ERROR: $*" >&2; ERRORS=$((ERRORS + 1)); }
warn() { echo "check-components: WARNING: $*" >&2; }

ROWS=$(mktemp "${TMPDIR:-/tmp}/pxb-reg.XXXXXX")
trap 'rm -f "$ROWS"' EXIT

awk -F'|' '
    { sub(/#.*/, "") }
    /^[[:space:]]*$/ { next }
    {
        for (i = 1; i <= NF; i++) gsub(/^[ \t]+|[ \t]+$/, "", $i)
        if ($1 == "") next
        print $1 "|" $2 "|" $4 "|" $6
    }
' "$REGISTRY" > "$ROWS"

norm() { printf '%s' "$1" | tr 'A-Z' 'a-z' | tr -cd 'a-z0-9'; }

while IFS='|' read -r name version ships path; do
    [ -n "$name" ] || continue

    case " $KNOWN_UNCERTAIN " in
        *" $name "*) [ "$ships" = "uncertain" ] && warn "$name: ships=uncertain (allowlisted; resolve and set yes/no)" ;;
        *) [ "$ships" = "uncertain" ] && err "$name: ships=uncertain and not allowlisted" ;;
    esac

    if [ "$ships" = "yes" ] && [ -z "$version" ]; then
        err "$name: ships=yes with an empty version (use 'unknown' if genuinely unversioned)"
    fi

    [ "$path" = "-" ] && continue

    if [ ! -e "$ROOT/$path" ]; then
        if [ "$name" = "libkmip" ]; then
            err "$name: $path missing — run 'git submodule update --init extra/libkmip'"
        else
            err "$name: path does not exist: $path"
        fi
        continue
    fi

    [ "$version" = "unknown" ] && continue

    base=$(basename "$path")
    case "$base" in
        *[0-9]*) ;;
        *) continue ;;
    esac

    nv=$(norm "$version")
    nb=$(norm "$base")
    case "$nb" in
        *"$nv"*) ;;
        *) err "$name: registry version '$version' not found in tree path '$path'" ;;
    esac
done < "$ROWS"

for d in "$ROOT"/extra/*/; do
    [ -d "$d" ] || continue
    dname=$(basename "$d")
    if ! awk -F'|' -v d="extra/${dname}" '
            $4 == d || index($4, d "/") == 1 { found = 1 }
            END { exit found ? 0 : 1 }' "$ROWS"; then
        err "extra/${dname} has no registry entry (add it, with ships=yes or ships=no)"
    fi
done

if [ "$ERRORS" -gt 0 ]; then
    echo "check-components: FAILED with ${ERRORS} error(s)" >&2
    exit 1
fi
echo "check-components: registry consistent with tree"
