#!/usr/bin/env bash
# Check the dynamic symbols of the Null allocator's operator new/delete (RTS_GAMEMEMORY_ENABLE=OFF) on Linux.
#
# GameMemoryNull.cpp hides the replaceable operator new/delete forms on ELF, forwards them to the process's
# operator new/delete with dlsym, and exports the (size_t, const char *, int) forms that NEW and newInstance use.
# This checks that none of the replaceable forms is in the dynamic symbol table, that the four NEW forms are,
# and that dlsym is imported for the forwarding.
#
# Usage:
#   ./scripts/qa/test-gamememorynull-exports-linux.sh <shared-object>
#       Check an engine shared object built with RTS_GAMEMEMORY_ENABLE=OFF (for example rlgenerals'
#       libgeneralsx.so).
#   ./scripts/qa/test-gamememorynull-exports-linux.sh --build-dir <cmake-build-dir>
#       Rebuild GameMemoryNull.cpp with its own command from <cmake-build-dir>/compile_commands.json, link it
#       alone into a temporary shared object, and check that. The build dir must be configured with
#       -DRTS_GAMEMEMORY_ENABLE=OFF, and the script must run where that build runs (inside the Docker builder
#       for the scripts/build/linux/docker-*.sh builds, since the commands use /work paths).
#
# Environment:
#   NM=nm             nm binary to use
#   CXX=c++           Compiler that links the temporary shared object (--build-dir only)

set -euo pipefail

NM="${NM:-nm}"
tmp_dir=""

usage() {
    sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//' >&2
    exit 2
}

build_shared_object() {
    local build_dir="$1" out_dir="$2"
    local db="${build_dir}/compile_commands.json"
    if [[ ! -f "$db" ]]; then
        echo "ERROR: ${db} not found (configure with CMAKE_EXPORT_COMPILE_COMMANDS=ON)" >&2
        exit 1
    fi

    # Write the directory and the argv of the first GameMemoryNull.cpp entry, NUL separated.
    local entry_file="$out_dir/command"
    if ! python3 - "$db" "$out_dir/GameMemoryNull.o" > "$entry_file" <<'EOF'
import json
import shlex
import sys

db_path, object_path = sys.argv[1], sys.argv[2]
for entry in json.load(open(db_path)):
    if entry["file"].endswith("/GameMemoryNull.cpp"):
        argv = entry.get("arguments") or shlex.split(entry["command"])
        out = []
        skip = False
        for arg in argv:
            if skip:
                skip = False
                continue
            if arg == "-o":
                skip = True
                out += ["-o", object_path]
                continue
            out.append(arg)
        if "-fPIC" not in out:
            out.append("-fPIC")
        sys.stdout.write("\0".join([entry["directory"]] + out))
        sys.exit(0)
sys.exit(1)
EOF
    then
        echo "ERROR: GameMemoryNull.cpp is not in ${db}; configure with -DRTS_GAMEMEMORY_ENABLE=OFF" >&2
        exit 1
    fi

    local -a argv
    mapfile -d '' argv < "$entry_file"
    local directory="${argv[0]}"
    echo "Compiling GameMemoryNull.cpp (from ${db})"
    (cd "$directory" && "${argv[@]:1}")
    "${CXX:-c++}" -shared -o "$out_dir/libgamememorynull.so" "$out_dir/GameMemoryNull.o"
}

check_shared_object() {
    local so="$1"
    local dynamic
    dynamic="$("$NM" -D "$so")"
    local defined
    defined="$(printf '%s\n' "$dynamic" | awk '$2 ~ /^[TtWwiV]$/ { print $3 }')"

    # size_t mangles as m (unsigned long) or j (unsigned int); take the one the NEW forms use.
    #
    # GeneralsX @bugfix cemlyn007 02/10/2026 Use a here-string (<<<), not `printf ... | grep -qx`:
    # grep -q stops reading as soon as it matches, so on a real engine .so (tens of thousands of
    # dynamic symbols) it closes its stdin while printf is still writing, and printf dies from
    # SIGPIPE. Under `set -o pipefail` (above) that makes the whole pipeline's exit status
    # non-zero even though grep found the match, so every "must be exported" check below always
    # reported FAIL, on every real binary, regardless of whether the engine was actually
    # correct -- this script was never run against a real build before (see the regression test
    # this now has, //rlgenerals/generalsx:gamememorynull_exports_test). A here-string has no
    # producer process to SIGPIPE.
    local s=m
    if grep -qx '_ZnwjPKci' <<<"$defined"; then
        s=j
    fi

    local failures=0
    local name
    for name in "_Znw${s}" "_Zna${s}" _ZdlPv _ZdaPv "_ZdlPv${s}" "_ZdaPv${s}"; do
        if grep -qx "$name" <<<"$defined"; then
            echo "FAIL: ${name} is exported; the replaceable form must be hidden"
            failures=$((failures + 1))
        else
            echo "ok:   ${name} is not exported"
        fi
    done
    for name in "_Znw${s}PKci" _ZdlPvPKci "_Zna${s}PKci" _ZdaPvPKci; do
        if grep -qx "$name" <<<"$defined"; then
            echo "ok:   ${name} is exported"
        else
            echo "FAIL: ${name} is not exported; NEW and newInstance in host code need it"
            failures=$((failures + 1))
        fi
    done
    if grep -Eq ' U dlsym(@|$)' <<<"$dynamic"; then
        echo "ok:   dlsym is imported for the forwarding"
    else
        echo "FAIL: dlsym is not imported; the hidden operators do not forward to the process's"
        failures=$((failures + 1))
    fi

    if [[ "$failures" -ne 0 ]]; then
        echo "ERROR: ${failures} check(s) failed for ${so}" >&2
        return 1
    fi
    echo "All checks passed for ${so}"
}

main() {
    [[ $# -ge 1 ]] || usage
    local so
    if [[ "$1" == "--build-dir" ]]; then
        [[ $# -eq 2 ]] || usage
        tmp_dir="$(mktemp -d)"
        trap 'rm -rf "$tmp_dir"' EXIT
        build_shared_object "$2" "$tmp_dir"
        so="$tmp_dir/libgamememorynull.so"
    else
        [[ $# -eq 1 ]] || usage
        so="$1"
    fi
    if [[ ! -f "$so" ]]; then
        echo "ERROR: ${so} not found" >&2
        exit 1
    fi
    check_shared_object "$so"
}

main "$@"
