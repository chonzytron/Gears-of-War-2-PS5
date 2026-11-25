#!/bin/bash
# (ps5/tools/bench: Linux test-build A/B runs and the Chapter 1 benchmark; see CLAUDE.md)
# A/B runs of the Linux build: each variant's logs go to logs/ab_<name>
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../.." || exit 1   # Gears2-Native
R=$HOME/.local/share/gow2-ps5-linux
# Your own disc image and Title Update 6 package (AB_ISO / AB_TU, else next to Gears2-Native):
ISO=${AB_ISO:-"../Gears of War 2 (USA) (En,Fr,De,Es,It,Zh,Ko,Pl,Ru,Cs,Hu).iso"}
TU=${AB_TU:-../TU_16L621D_000000K000000.00000000001G5}
T=${AB_SECONDS:-115}
while [[ $# -gt 0 ]]; do
  name=$1; flags=$2; shift 2
  mkdir -p "$R/logs/old"; mv "$R"/logs/*.bmp "$R"/logs/gow2_*.log "$R/logs/old/" 2>/dev/null
  rm -f /dev/shm/xenia_memory_* 2>/dev/null
  # A new campaign every run (AB_FRESH=0 keeps them): the campaign saves go, the profile (settings) stays.
  if [[ ${AB_FRESH:-1} != 0 ]]; then
    rm -rf "$R/UserData/B13EBABEBABEBABE/4D53082D/00000001/Gears2Checkpoint" \
           "$R/UserData/B13EBABEBABEBABE/4D53082D/Headers/00000001/Gears2Checkpoint.header"
  fi
  eval "extra=($flags)"
  timeout $T ps5/tools/linux_test.sh run "$ISO" "$TU" -- --ps5_perf_log=true --ps5_capture=${AB_CAPTURE:-5} \
    "${extra[@]}" > /dev/null 2>&1
  sleep 2
  rm -rf "$R/logs/ab_$name"; mkdir -p "$R/logs/ab_$name"
  mv "$R"/logs/*.bmp "$R"/logs/gow2_*.log "$R/logs/ab_$name/" 2>/dev/null
  echo "done $name"
done
echo all-done
