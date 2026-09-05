#!/bin/bash
# Export the census, Data/progression.json, from the built trees of THIS
# checkout or worktree, and rewrite Data/affixes.json in canonical form from
# what the affix library loaded. Run after any edit to the node library or
# the affix file and commit both; RiorsEdge.Data.Census.Fresh and
# RiorsEdge.Data.Affixes.Fresh are red until you do. Exit code is the
# commandlet's own; it refuses to rewrite an affix file that failed to load.
cd "$(git rev-parse --show-toplevel)" || exit 1
mkdir -p Saved/Logs
REPO=$(pwd -W 2>/dev/null || pwd)
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$REPO/riors_edge.uproject" -run=BreakerCensus -unattended -nop4 -nosplash -nullrhi -abslog="$REPO/Saved/Logs/census.log"
