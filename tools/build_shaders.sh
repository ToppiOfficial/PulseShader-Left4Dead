#!/bin/bash
# Compiles src/shaders/*.fxc into .vcs archives and .inc combo headers.
#   .inc -> build/inc      (consumed by the C++ shader)
#   .vcs -> dist/left4dead2/shaders/fxc
#
# Mirrors the pulse_add_shader_pair() function in CMakeLists.txt. Adding a
# shader variant means adding its prefix to PAIRS below.
set -e
set -o pipefail
cd "$(dirname "$0")/.."
ROOT="$PWD"

PAIRS="pulse_pbr pulse_girlsfrontline pulse_npr pulse_umamusume pulse_tooneye"

FXC="${FXC:-/c/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/fxc.exe}"
PREP="$ROOT/sdk/alienswarm-sdk/src/devtools/bin/fxc_prep.pl"
SDK_INC="$ROOT/sdk/valve-stdshaders"

WORK="$ROOT/build/shaderwork"
# Refuse to wipe the work dir out from under a shell sitting in it - that fails
# mid-script and leaves stale .inc files that then break the C++ build.
case "$PWD/" in
    "$WORK"/*) echo "error: run this from outside $WORK" >&2; exit 1 ;;
esac
rm -rf "$WORK"
mkdir -p "$WORK" "$ROOT/build/inc" "$ROOT/dist/left4dead2/shaders/fxc"

for prefix in $PAIRS; do
    # fxc_prep.pl writes filelistgen.txt into its cwd, so each pair needs its own.
    dir="$WORK/$prefix"
    mkdir -p "$dir"
    cp "$ROOT"/src/shaders/"$prefix"_vs30.fxc "$ROOT"/src/shaders/"$prefix"_ps30.fxc "$dir/"
    cp "$ROOT"/src/shaders/*.h "$dir/"
    cd "$dir"

    for stage in vs30 ps30; do
        perl "$PREP" "${prefix}_${stage}.fxc-----${prefix}_${stage}"
    done

    for stage in vs30 ps30; do
        python3 "$ROOT/tools/vcspack.py" filelistgen.txt --shader "${prefix}_${stage}" \
            --workdir . --out "$ROOT/dist/left4dead2/shaders/fxc/${prefix}_${stage}.vcs" \
            --fxc "$FXC" --include "$SDK_INC"
    done

    cp fxctmp9_tmp/*.inc "$ROOT/build/inc/"
    cd "$ROOT"
done

echo "=== shaders built ==="
ls -la "$ROOT/dist/left4dead2/shaders/fxc/" "$ROOT/build/inc/"
