# Sourced by the hooks. Extracts a field from the hook's stdin JSON without
# jq, which is not on every Windows seat; python is (status.py needs it).
PY=$(command -v python 2>/dev/null || command -v python3 2>/dev/null)
json_field() {  # json_field '<json>' '<dotted.path>'
  printf '%s' "$1" | "$PY" -c "import sys,json
d=json.load(sys.stdin)
for k in '$2'.split('.'):
    d=d.get(k,{}) if isinstance(d,dict) else {}
print(d if isinstance(d,str) else '')" 2>/dev/null
}
