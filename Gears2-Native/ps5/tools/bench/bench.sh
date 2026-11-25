#!/bin/bash
# (ps5/tools/bench: Linux test-build A/B runs and the Chapter 1 benchmark; see CLAUDE.md)
# bench.sh <name> [flags]  - Chapter 1 gameplay route, perf summary
cd "$(dirname "$(readlink -f "$0")")"
ROUTE='--gow2_launch="GearGame_P?loadcheckpoint=0?chapter=1 -nostartupmovies" --ps5_profile=true --ps5_input_script="30:a 40:lsup/6000 47:rsleft/500 48:lsup/6000 55:lsup/6000 62:rsright/700 63:lsup/6000 70:lsup/6000 77:rsleft/400 78:lsup/8000 87:lsup/8000 96:lsup/8000"'
AB_SECONDS=${BENCH_SECONDS:-125} AB_CAPTURE=10 AB_FRESH=0 ./ab.sh "b_$1" "$ROUTE $2" > /dev/null
python3 bench_sum.py "$HOME/.local/share/gow2-ps5-linux/logs/ab_b_$1" "$1"
