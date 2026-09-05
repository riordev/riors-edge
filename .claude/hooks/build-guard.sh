#!/bin/bash
# build-guard.sh — PreToolUse on Bash. Blocks a UE build while the editor is
# open unless -NoHotReloadFromIDE is passed. Live Coding holds the lock and
# the build fails late with a confusing message.
INPUT=$(cat); . "$(dirname "$0")/_json.sh"
CMD=$(json_field "$INPUT" tool_input.command)
echo "$CMD" | grep -q 'Build.bat' || exit 0
echo "$CMD" | grep -q 'NoHotReloadFromIDE' && exit 0
if tasklist 2>/dev/null | grep -qi 'UnrealEditor.exe'; then
  echo "Blocked: UnrealEditor.exe is running. Add -NoHotReloadFromIDE to the Build.bat command, or ask the owner to close the editor / press Ctrl+Alt+F11." >&2
  exit 2
fi
exit 0
