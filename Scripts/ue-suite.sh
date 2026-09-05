#!/bin/bash
# Run the RiorsEdge automation suite headless for THIS checkout or worktree.
# Results land in Saved/Logs/suite.log and nowhere else; read them with
# `python Scripts/status.py`. SoftQuit, never Quit.
cd "$(git rev-parse --show-toplevel)" || exit 1
mkdir -p Saved/Logs
REPO=$(pwd -W 2>/dev/null || pwd)
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$REPO/riors_edge.uproject" -ExecCmds="Automation RunTests RiorsEdge; SoftQuit" -unattended -nop4 -nosplash -nullrhi -abslog="$REPO/Saved/Logs/suite.log"
