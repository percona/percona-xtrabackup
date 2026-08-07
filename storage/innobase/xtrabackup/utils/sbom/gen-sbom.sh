#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

PKG=""
VER=""
DEST=""
REGISTRY="${SCRIPT_DIR}/components.txt"
ROOT="."
SCAN_LIBS=""

usage() {
    cat <<'EOF'
Usage: gen-sbom.sh --pkg NAME --version VER --dest DIR
                   [--registry FILE] [--root DIR] [--scan-libs DIR]
EOF
    exit 1
}

die() { echo "gen-sbom: ERROR: $*" >&2; exit 1; }

while [ $# -gt 0 ]; do
    case "$1" in
        --pkg)       PKG=${2:?};       shift 2 ;;
        --version)   VER=${2:?};       shift 2 ;;
        --dest)      DEST=${2:?};      shift 2 ;;
        --registry)  REGISTRY=${2:?};  shift 2 ;;
        --root)      ROOT=${2:?};      shift 2 ;;
        --scan-libs) SCAN_LIBS=${2:?}; shift 2 ;;
        --help)      usage ;;
        *) die "unknown argument: $1" ;;
    esac
done

[ -n "$PKG" ]  || die "--pkg is required"
[ -n "$VER" ]  || die "--version is required"
[ -n "$DEST" ] || die "--dest is required"
[ -f "$REGISTRY" ] || die "registry not found: $REGISTRY"

mkdir -p "$DEST"

TS=$(date -u -d "@${SOURCE_DATE_EPOCH:-}" +%Y-%m-%dT%H:%M:%SZ 2>/dev/null \
     || date -u +%Y-%m-%dT%H:%M:%SZ)

WORK=$(mktemp -d "${TMPDIR:-/tmp}/pxb-sbom.XXXXXX")
trap 'rm -rf "$WORK"' EXIT
COMPONENTS="${WORK}/components"
: > "$COMPONENTS"

awk -F'|' '
    { sub(/#.*/, "") }
    /^[[:space:]]*$/ { next }
    {
        for (i = 1; i <= NF; i++) { gsub(/^[ \t]+|[ \t]+$/, "", $i) }
        if ($4 != "yes") next
        if ($1 == "" || $2 == "" || $3 == "") {
            printf "gen-sbom: ERROR: ships=yes row missing name/version/license: [%s]\n", $0 > "/dev/stderr"
            bad = 1
            next
        }
        print $1 "|" $2 "|" $3 "|" $5 "|vendored"
    }
    END { if (bad) exit 1 }
' "$REGISTRY" >> "$COMPONENTS" || die "registry contains malformed ships=yes rows: $REGISTRY"

[ -s "$COMPONENTS" ] || die "registry produced no shipped components: $REGISTRY"

resolve_owner() {
    _p=$1
    if command -v rpm >/dev/null 2>&1; then
        _r=$(rpm -qf --qf '%{NAME}|%{VERSION}-%{RELEASE}|%{LICENSE}\n' "$_p" 2>/dev/null | head -1) || _r=""
        case $_r in
            *'|'*'|'*) printf '%s' "$_r"; return 0 ;;
        esac
    fi
    if command -v dpkg-query >/dev/null 2>&1; then
        _pk=$(dpkg -S "$_p" 2>/dev/null \
              | grep -v '^diversion by ' \
              | head -1 | cut -d: -f1) || _pk=""
        if [ -n "$_pk" ]; then
            _v=$(dpkg-query -W -f='${Version}\n' "$_pk" 2>/dev/null | head -1) || _v=""
            [ -n "$_v" ] || _v="unknown"
            printf '%s|%s|NOASSERTION' "$_pk" "$_v"
            return 0
        fi
    fi
    return 1
}

row_is_sane() {
    [ "$(printf '%s' "$1" | awk -F'|' 'END { print NF }')" = "3" ] || return 1
    case $1 in
        *'	'*) return 1 ;;
    esac
    printf '%s' "$1" | awk -F'|' '{ exit ($1 == "" || $2 == "") ? 1 : 0 }'
}

if [ -n "$SCAN_LIBS" ]; then
    [ -d "$SCAN_LIBS" ] || die "--scan-libs directory not found: $SCAN_LIBS"

    HOSTMAP="${WORK}/hostmap"
    ldconfig -p 2>/dev/null | awk '/=>/ { print $NF }' | while IFS= read -r _p; do
        _rp=$(readlink -f "$_p" 2>/dev/null) || continue
        [ -f "$_rp" ] || continue
        printf '%s|%s\n' "$(basename "$_rp")" "$_rp"
    done | sort -u > "$HOSTMAP" || true

    for f in "$SCAN_LIBS"/*; do
        [ -L "$f" ] && continue
        [ -f "$f" ] || continue
        base=$(basename "$f")
        origin=$(awk -F'|' -v b="$base" '$1 == b { print $2; exit }' "$HOSTMAP") || true
        if [ -z "$origin" ]; then
            origin=$(ldconfig -p 2>/dev/null \
                     | awk -v b="$base" '$1 == b { print $NF; exit }') || true
            [ -n "$origin" ] && origin=$(readlink -f "$origin" 2>/dev/null) || true
        fi
        if [ -z "$origin" ] || [ ! -e "$origin" ]; then
            echo "gen-sbom: WARNING: no host origin for ${base}; recorded as unknown" >&2
            printf '%s|unknown|NOASSERTION|shared-lib|host\n' "$base" >> "$COMPONENTS"
            continue
        fi
        if row=$(resolve_owner "$origin") && row_is_sane "$row"; then
            printf '%s|shared-lib|host\n' "$row" >> "$COMPONENTS"
        else
            echo "gen-sbom: WARNING: ${base} owned by no host package (or the" \
                 "package database returned an unusable record); recorded as unknown" >&2
            printf '%s|unknown|NOASSERTION|shared-lib|host\n' "$base" >> "$COMPONENTS"
        fi
    done
fi

tr '\t' ' ' < "$COMPONENTS" | tr -d '\000-\010\013\014\016-\037\177' > "${COMPONENTS}.clean"
mv "${COMPONENTS}.clean" "$COMPONENTS"

sort -u -o "$COMPONENTS" "$COMPONENTS"
COUNT=$(wc -l < "$COMPONENTS" | tr -d ' ')

DUPES=$(awk -F'|' '{ id = $1; gsub(/[^A-Za-z0-9.-]/, "-", id); print id }' "$COMPONENTS" \
        | sort | uniq -d)
[ -z "$DUPES" ] || die "SPDXID collision between components: $(echo "$DUPES" | tr '\n' ' ')"

uuid_from_content() {
    _seed="${PKG}|${VER}|$(cat "$COMPONENTS")"
    _hex=""
    if command -v sha256sum >/dev/null 2>&1; then
        _hex=$(printf '%s' "$_seed" | sha256sum | cut -c1-32)
    elif command -v shasum >/dev/null 2>&1; then
        _hex=$(printf '%s' "$_seed" | shasum -a 256 | cut -c1-32)
    fi
    if [ -z "$_hex" ]; then
        _hex=$(printf '%s' "$_seed" | awk '{
            h = 0
            for (i = 1; i <= length($0); i++) h = (h * 31 + index($0, substr($0, i, 1))) % 4294967296
            printf "%08x%08x%08x%08x", h, (h*7)%4294967296, (h*13)%4294967296, (h*17)%4294967296
        }')
    fi
    printf '%s-%s-%s-%s-%s' \
        "$(printf '%s' "$_hex" | cut -c1-8)"   "$(printf '%s' "$_hex" | cut -c9-12)" \
        "$(printf '%s' "$_hex" | cut -c13-16)" "$(printf '%s' "$_hex" | cut -c17-20)" \
        "$(printf '%s' "$_hex" | cut -c21-32)"
}
UUID=$(uuid_from_content)

emit() {
    awk -F'|' -v pkg="$PKG" -v ver="$VER" -v ts="$TS" -v uuid="$UUID" \
        -v mode="$1" '
    function esc(s) { gsub(/\\/, "\\\\", s); gsub(/"/, "\\\"", s); return s }
    function pct(s,   i, c, o) {
        o = ""
        for (i = 1; i <= length(s); i++) {
            c = substr(s, i, 1)
            if (c ~ /[A-Za-z0-9._~:-]/) o = o c
            else o = o sprintf("%%%02X", ORD[c])
        }
        return o
    }
    function purl(n, v) { return "pkg:generic/" pct(n) (v == "unknown" ? "" : "@" pct(v)) }
    function spdxid(n) { gsub(/[^A-Za-z0-9.-]/, "-", n); return n }
    function is_expr(l) { return (l ~ / OR / || l ~ / AND /) }
    BEGIN { for (i = 0; i < 256; i++) ORD[sprintf("%c", i)] = i }
    {
        n[NR] = $1; v[NR] = $2; l[NR] = $3; k[NR] = $4; o[NR] = $5
    }
    END {
        if (mode == "spdx") {
            print "{"
            print "  \"spdxVersion\": \"SPDX-2.3\","
            print "  \"dataLicense\": \"CC0-1.0\","
            print "  \"SPDXID\": \"SPDXRef-DOCUMENT\","
            printf "  \"name\": \"%s-%s\",\n", esc(pkg), esc(ver)
            printf "  \"documentNamespace\": \"https://percona.com/spdx/%s-%s-%s\",\n", esc(pkg), esc(ver), uuid
            print "  \"creationInfo\": {"
            printf "    \"created\": \"%s\",\n", ts
            print "    \"creators\": [ \"Tool: pxb-gen-sbom\", \"Organization: Percona LLC\" ]"
            print "  },"
            print "  \"packages\": ["
            printf "    { \"SPDXID\": \"SPDXRef-Package-ROOT\", \"name\": \"%s\", \"versionInfo\": \"%s\",", esc(pkg), esc(ver)
            print " \"downloadLocation\": \"NOASSERTION\", \"filesAnalyzed\": false,"
            print "      \"licenseConcluded\": \"GPL-2.0-only\", \"licenseDeclared\": \"GPL-2.0-only\", \"copyrightText\": \"NOASSERTION\" }"
            for (i = 1; i <= NR; i++) {
                printf "    ,{ \"SPDXID\": \"SPDXRef-Package-%s\", \"name\": \"%s\", \"versionInfo\": \"%s\",\n", spdxid(n[i]), esc(n[i]), esc(v[i])
                print "      \"downloadLocation\": \"NOASSERTION\", \"filesAnalyzed\": false,"
                printf "      \"licenseConcluded\": \"%s\", \"licenseDeclared\": \"%s\", \"copyrightText\": \"NOASSERTION\",\n", esc(l[i]), esc(l[i])
                printf "      \"comment\": \"linkage=%s origin=%s\",\n", esc(k[i]), esc(o[i])
                printf "      \"externalRefs\": [ { \"referenceCategory\": \"PACKAGE-MANAGER\", \"referenceType\": \"purl\", \"referenceLocator\": \"%s\" } ] }\n", esc(purl(n[i], v[i]))
            }
            print "  ],"
            print "  \"relationships\": ["
            print "    { \"spdxElementId\": \"SPDXRef-DOCUMENT\", \"relatedSpdxElement\": \"SPDXRef-Package-ROOT\", \"relationshipType\": \"DESCRIBES\" }"
            for (i = 1; i <= NR; i++) {
                printf "    ,{ \"spdxElementId\": \"SPDXRef-Package-ROOT\", \"relatedSpdxElement\": \"SPDXRef-Package-%s\", \"relationshipType\": \"CONTAINS\" }\n", spdxid(n[i])
            }
            print "  ]"
            print "}"
        } else if (mode == "cdx") {
            print "{"
            print "  \"bomFormat\": \"CycloneDX\","
            print "  \"specVersion\": \"1.5\","
            printf "  \"serialNumber\": \"urn:uuid:%s\",\n", uuid
            print "  \"version\": 1,"
            print "  \"metadata\": {"
            printf "    \"timestamp\": \"%s\",\n", ts
            print "    \"tools\": [ { \"vendor\": \"Percona\", \"name\": \"pxb-gen-sbom\", \"version\": \"1.0\" } ],"
            printf "    \"component\": { \"type\": \"application\", \"bom-ref\": \"%s\", \"name\": \"%s\", \"version\": \"%s\",", esc(purl(pkg, ver)), esc(pkg), esc(ver)
            print " \"licenses\": [ { \"license\": { \"id\": \"GPL-2.0-only\" } } ] }"
            print "  },"
            print "  \"components\": ["
            for (i = 1; i <= NR; i++) {
                printf "%s", (i == 1 ? "    " : "    ,")
                printf "{ \"type\": \"library\", \"bom-ref\": \"%s\", \"name\": \"%s\", \"version\": \"%s\",\n", esc(purl(n[i], v[i])), esc(n[i]), esc(v[i])
                printf "      \"purl\": \"%s\",\n", esc(purl(n[i], v[i]))
                if (l[i] == "NOASSERTION")
                    print "      \"licenses\": [],"
                else if (is_expr(l[i]))
                    printf "      \"licenses\": [ { \"expression\": \"%s\" } ],\n", esc(l[i])
                else
                    printf "      \"licenses\": [ { \"license\": { \"id\": \"%s\" } } ],\n", esc(l[i])
                printf "      \"properties\": [ { \"name\": \"pxb:linkage\", \"value\": \"%s\" }, { \"name\": \"pxb:origin\", \"value\": \"%s\" } ] }\n", esc(k[i]), esc(o[i])
            }
            print "  ]"
            print "}"
        } else if (mode == "table") {
            printf "%-22s %-42s %-30s %-12s %s\n", "NAME", "VERSION", "LICENSE", "LINKAGE", "ORIGIN"
            for (i = 1; i <= NR; i++)
                printf "%-22s %-42s %-30s %-12s %s\n", n[i], v[i], l[i], k[i], o[i]
        } else if (mode == "licenses") {
            for (i = 1; i <= NR; i++) printf "%s %s %s\n", n[i], v[i], l[i]
        }
    }' "$COMPONENTS"
}

emit spdx     > "${DEST}/${PKG}.spdx.json"
emit cdx      > "${DEST}/${PKG}.cdx.json"
emit table    > "${DEST}/${PKG}.sbom.txt"
emit licenses > "${DEST}/${PKG}.licenses.txt"

chmod 0755 "${DEST}"
chmod 0644 "${DEST}/${PKG}.spdx.json" "${DEST}/${PKG}.cdx.json" \
           "${DEST}/${PKG}.sbom.txt"  "${DEST}/${PKG}.licenses.txt"

echo "gen-sbom: wrote ${COUNT} components for ${PKG} ${VER} to ${DEST}"
