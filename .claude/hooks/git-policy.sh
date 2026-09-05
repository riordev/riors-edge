#!/bin/bash
# git-policy.sh — PreToolUse on Bash(git *). Exit 2 blocks; stderr is fed
# back to Claude. Deterministic: a rule in a hook cannot be forgotten.
INPUT=$(cat); . "$(dirname "$0")/_json.sh"
CMD=$(json_field "$INPUT" tool_input.command)
CWD=$(json_field "$INPUT" cwd); [ -n "$CWD" ] && cd "$CWD" 2>/dev/null

# This hook only fires for git commands, so an empty read is a parse
# failure, and a policy hook does not guess. Fail closed, loudly.
if [ -z "$CMD" ]; then
  echo "git-policy: could not read the command from the hook input (see .claude/hooks/_json.sh). Refusing rather than passing unchecked." >&2
  exit 2
fi

# 1. Stage by name, never the sweep.
if echo "$CMD" | grep -Eq 'git\s+add\s+(-A|--all|\.|\*)(\s|$)'; then
  echo "Blocked: stage files by name. 'git add -A' / 'git add .' can sweep another lane's uncommitted work." >&2
  exit 2
fi

# 2. Never force.
if echo "$CMD" | grep -Eq 'git\s+push.*(--force|-f\b|--force-with-lease)'; then
  echo "Blocked: never force-push. Rebase onto origin/main and push a fast-forward, or report the refusal." >&2
  exit 2
fi

# 3. A push that changes Source/ needs a suite run newer than the newest
#    edit. Docs-only and config-only pushes are exempt. (Where a session
#    runs — worktree or checkout — is the app's and the owner's business.)
if echo "$CMD" | grep -Eq 'git\s+push'; then
  git fetch -q origin main 2>/dev/null
  CODE_CHANGED=$(git diff --name-only origin/main...HEAD -- Source/ 2>/dev/null | head -1)
  if [ -n "$CODE_CHANGED" ]; then
    LOG="Saved/Logs/suite.log"
    if [ ! -f "$LOG" ]; then
      echo "Blocked: this push changes Source/ and there is no Saved/Logs/suite.log here. Run /cycle first." >&2
      exit 2
    fi
    FIND=$(command -v /usr/bin/find 2>/dev/null || command -v find)
    NEWER=$("$FIND" Source -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.cs' \) -newer "$LOG" 2>/dev/null | head -1)
    if [ -n "$NEWER" ]; then
      echo "Blocked: $NEWER is newer than suite.log. Run /cycle (BUILD -> SUITE -> status.py) before pushing." >&2
      exit 2
    fi
  fi
fi
exit 0
