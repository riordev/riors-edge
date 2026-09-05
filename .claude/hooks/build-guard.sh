#!/bin/bash
# build-guard.sh — PreToolUse on Bash. Refuses a UE build in the MAIN
# checkout while the editor is open (Live Coding holds the lock and the build
# fails late). A worktree build is allowed: its -NoHotReloadFromIDE makes the
# lock a false positive.
INPUT=$(cat); . "$(dirname "$0")/_json.sh"
CMD=$(json_field "$INPUT" tool_input.command)
echo "$CMD" | grep -Eq 'Build\.bat|ue-build\.sh' || exit 0
CWD=$(json_field "$INPUT" cwd); [ -n "$CWD" ] && cd "$CWD" 2>/dev/null
[ "$(git rev-parse --git-common-dir 2>/dev/null)" != ".git" ] && exit 0   # a worktree: allowed
if tasklist 2>/dev/null | grep -qi 'UnrealEditor.exe'; then
  echo "Blocked: UnrealEditor.exe is open on this checkout and this is not a worktree, so Live Coding holds the build lock. Close the editor (or Ctrl+Alt+F11), or run this lane in a worktree." >&2
  exit 2
fi
exit 0
