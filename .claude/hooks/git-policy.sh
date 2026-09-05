#!/bin/bash
# git-policy.sh — PreToolUse on Bash(git *). Exit 2 blocks; stderr is fed
# back to Claude. Enforces the git rules CLAUDE.md says keep arriving in new
# shapes. Deterministic: a rule in a hook cannot be forgotten mid-session.
INPUT=$(cat); . "$(dirname "$0")/_json.sh"
CMD=$(json_field "$INPUT" tool_input.command)
[ -z "$CMD" ] && exit 0
# Hooks run wherever Claude Code launched; the session's own directory (the
# worktree) arrives in the JSON. Operate there.
CWD=$(json_field "$INPUT" cwd); [ -n "$CWD" ] && cd "$CWD" 2>/dev/null

# 1. Stage by name, never the sweep. The sweep committed another lane's
#    uncommitted files "as found" once.
if echo "$CMD" | grep -Eq 'git\s+add\s+(-A|--all|\.|\*)(\s|$)'; then
  echo "Blocked: stage files by name. 'git add -A' / 'git add .' can sweep another lane's uncommitted work." >&2
  exit 2
fi

# 2. Never force. A lane's push either fast-forwards main or is refused.
if echo "$CMD" | grep -Eq 'git\s+push.*(--force|-f\b|--force-with-lease)'; then
  echo "Blocked: never force-push. Rebase onto origin/main and push a fast-forward, or report the refusal." >&2
  exit 2
fi

if echo "$CMD" | grep -Eq 'git\s+push'; then
  # 3. Never push from the owner's main checkout. Lanes live in worktrees
  #    (the desktop app makes one per session).
  COMMON=$(git rev-parse --git-common-dir 2>/dev/null)
  if [ "$COMMON" = ".git" ]; then
    echo "Blocked: this is the owner's main checkout, not a lane worktree. Lanes push from their own worktree; the owner pushes from a terminal." >&2
    exit 2
  fi
  # 4. A push that changes code needs a suite run newer than the newest edit.
  #    Docs-only and config-only pushes are exempt.
  git fetch -q origin main 2>/dev/null
  CODE_CHANGED=$(git diff --name-only origin/main...HEAD -- Source/ 2>/dev/null | head -1)
  if [ -n "$CODE_CHANGED" ]; then
    LOG="Saved/Logs/suite.log"
    if [ ! -f "$LOG" ]; then
      echo "Blocked: this push changes Source/ and there is no Saved/Logs/suite.log in this worktree. Run /cycle first." >&2
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
