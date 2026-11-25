#!/usr/bin/env bash
# Gears of War 2 for PS5: the same program built for Linux, to test changes on
# a PC before they go on a console (ps5/BUILD.md, "On Linux"). After the Dead
# Space 2 PS5 port's linux_test.sh.
#
#   ps5/tools/linux_test.sh build                        # configure (once) and compile
#   ps5/tools/linux_test.sh run <files...> [-- flags]    # run it
#
# <files> are your own game files, as the PS5 app's install/ folder takes them
# (package/install/README.txt): the disc image (.iso) and the Title Update 6
# package, or a folder that holds them. They are only read (linked), never
# copied into the repository.
#
# The program finds everything next to itself, as on the PS5, so it runs from
# a folder of its own (GOW2_LINUX_RUN, default ~/.local/share/gow2-ps5-linux):
# the program (a hard link to the build), install/ (links to <files>), game/,
# UserData/ (saves, unpacked DLC, caches), logs/ and gow2.toml, which it keeps
# between runs. Flags after -- go to the program (--name=value, as in gow2.toml).
#
# Compiled for the PS5's processor (-march=znver2: AMD Zen 2 or newer), as the
# console build is, so the program does what it does there.
#
# Environment: GOW2_LINUX_BUILD (default out/linux-ps5), GOW2_LINUX_JOBS
# (default: one per 2.5 GB of memory: the recompiled game's files are big).
set -euo pipefail

g2=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)   # Gears2-Native
build=${GOW2_LINUX_BUILD:-$g2/out/linux-ps5}
run_dir=${GOW2_LINUX_RUN:-$HOME/.local/share/gow2-ps5-linux}

jobs() {
    if [[ -n ${GOW2_LINUX_JOBS:-} ]]; then echo "$GOW2_LINUX_JOBS"; return; fi
    local kb cpus n
    kb=$(awk '/MemTotal/ {print $2}' /proc/meminfo)
    cpus=$(nproc)
    n=$(( kb / 2600000 ))
    (( n < 1 )) && n=1
    (( n > cpus )) && n=$cpus
    echo "$n"
}

case ${1:-} in
build)
    [[ -f $g2/third_party/rexglue-sdk-ps5/include/rex/ps5_compat.h ]] || "$g2/ps5/tools/fetch_sdk.sh"
    if [[ ! -f $build/build.ninja ]]; then
        cmake -S "$g2/ps5" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
            -DGOW2_PS5_LINUX=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
            -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
            -DSDL_X11_XTEST=OFF \
            "-DCMAKE_C_FLAGS=-march=znver2" "-DCMAKE_CXX_FLAGS=-march=znver2"
    fi
    ninja -C "$build" -j "$(jobs)" gow2
    echo "==> built $build/gow2"
    ;;
run)
    shift
    files=()
    while [[ $# -gt 0 && $1 != -- ]]; do files+=("$1"); shift; done
    [[ ${1:-} == -- ]] && shift
    (( ${#files[@]} )) || { echo "usage: $0 run <disc image> <title update> | <folder> [-- flags]" >&2; exit 2; }
    [[ -x $build/gow2 ]] || { echo "no $build/gow2: run '$0 build' first" >&2; exit 2; }
    mkdir -p "$run_dir/install"
    ln -f "$build/gow2" "$run_dir/gow2" 2>/dev/null || cp -f "$build/gow2" "$run_dir/gow2"
    # install/ holds links to the player's files (only links are replaced here).
    find "$run_dir/install" -maxdepth 1 -type l -delete
    for f in "${files[@]}"; do
        f=$(realpath -- "$f")
        case $f/ in "$g2"/*) echo "keep your game files outside Gears2-Native: $f" >&2; exit 2 ;; esac
        if [[ -d $f ]]; then
            for g in "$f"/*; do [[ -f $g ]] && ln -sfn "$g" "$run_dir/install/$(basename -- "$g")"; done
        else
            ln -sfn "$f" "$run_dir/install/$(basename -- "$f")"
        fi
    done
    [[ -f $run_dir/gow2.toml ]] || cp "$g2/ps5/package/gow2.example.toml" "$run_dir/gow2.toml"
    # Guest memory is a file in /dev/shm, removed when the program ends; one left by a run that was killed fills it,
    # and the next run then dies of a bus error as soon as the game touches memory that can't be backed.
    if ! pgrep -x gow2 > /dev/null; then
        rm -f /dev/shm/xenia_memory_*
    fi
    echo "==> running from $run_dir (settings: $run_dir/gow2.toml, logs: $run_dir/logs)"
    cd "$run_dir"
    exec ./gow2 --user_data_root="$run_dir/UserData" "$@"
    ;;
*)
    sed -n '2,24p' "$0" | sed 's/^# \{0,1\}//'
    exit 2
    ;;
esac
