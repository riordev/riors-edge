#!/bin/bash
# Build RiorsEdgeEditor for THIS checkout or worktree. Exit code is the
# build's own — never pipe this through tail. -NoHotReloadFromIDE is always
# on: in a worktree the Live Coding lock is a false positive (it keys off the
# shared UnrealEditor.exe, not this tree's DLL); in the main checkout the
# build-guard hook refuses the build while the editor is open instead.
set -o pipefail
cd "$(git rev-parse --show-toplevel)" || exit 1
git lfs pull >/dev/null 2>&1 || true     # a fresh worktree may hold pointer files
REPO=$(pwd -W 2>/dev/null || pwd)
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" RiorsEdgeEditor Win64 Development -Project="$REPO/riors_edge.uproject" -WaitMutex -NoHotReloadFromIDE
