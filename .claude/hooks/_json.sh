# Sourced by the hooks. Extracts one field from the hook's stdin JSON.
# Tries python, then falls back to sed so a seat without python still gets a
# working hook. A hook that cannot read its input must not fail open.
PY=$(command -v python 2>/dev/null || command -v python3 2>/dev/null)
json_field() {  # json_field '<json>' '<dotted.path>'  -> value on stdout
  local val="" key="${2##*.}"
  if [ -n "$PY" ]; then
    val=$(printf '%s' "$1" | "$PY" -c "import sys,json
d=json.load(sys.stdin)
for k in '$2'.split('.'):
    d=d.get(k,{}) if isinstance(d,dict) else {}
print(d if isinstance(d,str) else '')" 2>/dev/null)
  fi
  if [ -z "$val" ]; then
    val=$(printf '%s' "$1" | tr -d '\n' \
      | sed -E -n 's/.*"'"$key"'"[[:space:]]*:[[:space:]]*"(([^"\\]|\\.)*)".*/\1/p' \
      | sed -e 's/\\"/"/g' -e 's/\\n/ /g' -e 's/\\\\/\\/g')
  fi
  printf '%s' "$val"
}
