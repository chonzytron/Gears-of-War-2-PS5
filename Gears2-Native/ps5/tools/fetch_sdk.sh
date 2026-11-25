#!/usr/bin/env bash
# Gears of War 2 for PS5: the ReXGlue SDK source the PS5 (and Linux test) build compiles.
#
#   ps5/tools/fetch_sdk.sh [directory]          (run from Gears2-Native)
#
# Clones rexglue-sdk at the tag COGBox's PC build pins (ps5/sdk/REXGLUE_BASE.txt) with its
# third-party submodules, moves the submodules COGBox moves (patches/sdk-pins.txt), then applies:
#   1. COGBox's SDK patches, as its PC runtime has them (patches/README.md): rexglue-sdk-full.diff
#      (patches 1-45), then sdk-patch46 and later in number order, with README.md's exceptions.
#   2. ps5/sdk/rexglue_ps5.patch   the PS5 platform layer (when it exists), on top of 1.
#   3. ps5/sdk/ffmpeg_ps5.patch    FFmpeg's configuration for the PS5 (in thirdparty/FFmpeg).
# Safe to run again: what is already applied is left alone (a stamp records step 1).
set -euo pipefail

g2=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
patches=$g2/patches
sdk=${1:-$g2/third_party/rexglue-sdk-ps5}

pin() { sed -n "s/^$1 *= *//p" "$g2/ps5/sdk/REXGLUE_BASE.txt" | head -1 | awk '{print $1}'; }
url=$(pin upstream_repo)
tag=$(pin upstream_tag)
commit=$(pin upstream_commit)
[[ -n $url && -n $tag && -n $commit ]] || { echo "ps5/sdk/REXGLUE_BASE.txt has no pin" >&2; exit 2; }

if [[ ! -d $sdk/.git ]]; then
    mkdir -p "$sdk"
    git -C "$sdk" init -q
    git -C "$sdk" remote add origin "$url"
    echo "==> [sdk] fetching rexglue-sdk $tag"
    git -C "$sdk" fetch -q --depth 1 origin "refs/tags/$tag"
    git -C "$sdk" -c advice.detachedHead=false checkout -q FETCH_HEAD
fi
# (the pinned commit itself, or, once COGBox's patches are committed on it, that commit's parent)
git -C "$sdk" merge-base --is-ancestor "$commit" HEAD 2>/dev/null ||
    { echo "$sdk is at $(git -C "$sdk" rev-parse HEAD), not on the pinned $commit" >&2; exit 2; }

stamp=$sdk/.cogbox-patches-applied
if [[ ! -f $stamp ]]; then
    echo "==> [sdk] third-party submodules"
    git -C "$sdk" submodule update --init --recursive --depth 1 --jobs 8
    # COGBox moves some submodules ahead of the SDK (patches/sdk-pins.txt: "path tag").
    while read -r path ref; do
        [[ -z $path || $path == \#* ]] && continue
        echo "==> [sdk] $path -> $ref"
        git -C "$sdk/$path" fetch -q --depth 1 origin "refs/tags/$ref:refs/tags/$ref"
        git -C "$sdk/$path" -c advice.detachedHead=false checkout -q "$ref"
        git -C "$sdk/$path" submodule update --init --recursive --depth 1 --jobs 8 || true
    done < "$patches/sdk-pins.txt"

    # COGBox's patches in the order patches/README.md gives. Skipped: 15-37 (inside the full
    # diff), 16 (never built). 109 comes after 111 (README: "0.6.4, after 111").
    list=(rexglue-sdk-full.diff)
    mapfile -t numbered < <(cd "$patches" && ls sdk-patch*.diff | grep -v 'sdk-patch108-ffmpeg' |
        awk 'match($0,/sdk-patch([0-9]+)([a-z]?)/,m){ if (m[1]+0>=46) printf "%04d%s %s\n", m[1], (m[2]==""?"0":m[2]), $0 }' |
        sort | awk '{print $2}')
    for p in "${numbered[@]}"; do
        [[ $p == sdk-patch109-* ]] && continue
        list+=("$p")
        [[ $p == sdk-patch111-* ]] && list+=(sdk-patch109-codegen-fixes.diff)
    done

    # Most patches are made in the SDK root (a/ b/ prefixes). A few are not (README notes):
    # some carry the PC tree's third_party/rexglue-sdk/ prefix, some name a bare file made
    # inside its own folder. path_args prints the git apply arguments that place them.
    path_args() {
        local f=$1 first
        first=$(grep -m1 -E '^\+\+\+ ' "$f" | sed -E 's/^\+\+\+ //; s/\t.*//')
        if [[ $first == b/third_party/rexglue-sdk/* ]]; then
            echo "-p3"
        elif [[ $first != */* ]]; then
            local hit
            hit=$(git -C "$sdk" ls-files | grep -m1 -E "(^|/)${first//./\\.}$" || true)
            [[ -n $hit ]] && echo "-p0 --directory=$(dirname "$hit")" || echo "-p1"
        else
            echo "-p1"
        fi
    }
    # Each applied patch is noted in $progress, so a run stopped part way goes on from there
    # (a later patch can change the lines an earlier one did: re-checking them doesn't work).
    progress=$sdk/.cogbox-patches-progress
    touch "$progress"
    for p in "${list[@]}"; do
        f=$patches/$p
        if grep -qxF "$p" "$progress"; then
            continue
        fi
        read -r -a pa <<< "$(path_args "$f")"
        if git -C "$sdk" apply "${pa[@]}" --whitespace=nowarn "$f" 2>/dev/null; then
            echo "==> [sdk] $p: applied ${pa[*]}"
        elif git -C "$sdk" apply "${pa[@]}" --ignore-whitespace --whitespace=nowarn "$f" 2>/dev/null; then
            echo "==> [sdk] $p: applied ${pa[*]} (whitespace ignored)"
        elif git -C "$sdk" apply "${pa[@]}" --ignore-whitespace --whitespace=nowarn --exclude='tests/*' "$f" 2>/dev/null; then
            # The PS5 build compiles no SDK unit tests; COGBox's test files drift between patches.
            echo "==> [sdk] $p: applied ${pa[*]} WITHOUT its tests/ hunks (they don't apply; not built here)"
        elif git -C "$sdk" apply "${pa[@]}" --check --reverse --ignore-whitespace --whitespace=nowarn "$f" 2>/dev/null; then
            # Its changes are already there (COGBox folded it into an earlier patch, e.g. 56b into 56).
            echo "==> [sdk] $p: already contained in the patches before it"
        else
            echo "!! [sdk] $p does not apply:" >&2
            git -C "$sdk" apply "${pa[@]}" --check --ignore-whitespace --whitespace=nowarn "$f" >&2 || true
            exit 2
        fi
        echo "$p" >> "$progress"
    done
    # The XMA decoder's half that lives in the FFmpeg submodule (README: patch 108, CRLF in Windows).
    ff=$patches/sdk-patch108-ffmpeg-xmaframes.diff
    if git -C "$sdk/thirdparty/FFmpeg" apply --check --reverse --ignore-whitespace "$ff" 2>/dev/null; then
        echo "==> [sdk] sdk-patch108-ffmpeg-xmaframes.diff: already applied"
    else
        git -C "$sdk/thirdparty/FFmpeg" apply --ignore-whitespace --whitespace=nowarn "$ff"
        echo "==> [sdk] sdk-patch108-ffmpeg-xmaframes.diff: applied (thirdparty/FFmpeg)"
    fi
    # The clean COGBox tree, committed and tagged: rexglue_ps5.patch is made against it
    # (git -c core.abbrev=8 diff gow2-patch-base > ps5/sdk/rexglue_ps5.patch).
    git -C "$sdk" add -A
    git -C "$sdk" -c user.name=gow2-ps5 -c user.email=gow2-ps5@localhost commit -q -m "COGBox SDK patches" --no-verify
    git -C "$sdk" tag -f gow2-patch-base >/dev/null
    date -u +%FT%TZ > "$stamp"
fi

if [[ -f $g2/ps5/sdk/rexglue_ps5.patch ]]; then
    f=$g2/ps5/sdk/rexglue_ps5.patch
    if git -C "$sdk" apply --check --reverse --whitespace=nowarn "$f" 2>/dev/null; then
        echo "==> [sdk] rexglue_ps5.patch: already applied"
    else
        git -C "$sdk" apply --whitespace=nowarn "$f"
        echo "==> [sdk] rexglue_ps5.patch: applied"
    fi
fi
if [[ -f $g2/ps5/sdk/ffmpeg_ps5.patch ]]; then
    f=$g2/ps5/sdk/ffmpeg_ps5.patch
    if git -C "$sdk/thirdparty/FFmpeg" apply --check --reverse --whitespace=nowarn "$f" 2>/dev/null; then
        echo "==> [sdk] ffmpeg_ps5.patch: already applied"
    else
        git -C "$sdk/thirdparty/FFmpeg" apply --whitespace=nowarn "$f"
        echo "==> [sdk] ffmpeg_ps5.patch: applied"
    fi
fi
echo "==> [sdk] ready: $sdk"
