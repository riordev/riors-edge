#!/bin/bash
# Capture harness for THIS checkout or worktree. First argument is the map
# (Anchor|Gym|Fernhall); everything after is passed through as -Breaker
# switches. Frames land in Saved/Screenshots/breaker_NN.png.
cd "$(git rev-parse --show-toplevel)" || exit 1
MAP=${1:-Anchor}; shift
REPO=$(pwd -W 2>/dev/null || pwd)
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$REPO/riors_edge.uproject" -game -windowed -ResX=1920 -ResY=1080 -BreakerAutoPlay="$MAP" -BreakerScreenshots=4 "$@"
