"""Summarize ordinary-play recorder files; no third-party dependencies."""
import argparse
import json
from pathlib import Path


def read_rows(path):
    rows = []
    for number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
        if line.strip():
            try:
                row = json.loads(line)
                if not isinstance(row, dict) or not isinstance(row.get("seconds"), (int, float)):
                    raise ValueError("missing numeric seconds")
                rows.append(row)
            except (ValueError, TypeError) as error:
                raise ValueError(f"{path}:{number}: {error}") from error
    return rows


def report(path):
    rows = read_rows(path)
    print(f"\n{path}")
    if not rows:
        print("No player samples recorded.")
        return
    last = rows[-1]
    print(f"Recorded span: {last['seconds'] - rows[0]['seconds']:.1f}s; "
          f"kills {last['kills']}; deaths {last['deaths']}; interrupted casts {last['interrupted_casts']}")
    print("Elapsed intervals include menus and pauses; they are not active combat times.")
    print("Start | Seconds | Level/area | Wave | XP | Core/doctrine | Objective")
    start = 0
    def key(row):
        return (row.get("map"), row.get("objective"), row.get("wave"), row.get("level"), row.get("rift_complete"))
    for index in range(1, len(rows) + 1):
        if index < len(rows) and key(rows[index]) == key(rows[start]):
            continue
        row = rows[start]
        end_seconds = rows[index]["seconds"] if index < len(rows) else last["seconds"]
        print(f"{row['seconds']:.0f}s | {end_seconds-row['seconds']:.0f} | "
              f"{row['level']}/{row['area_level']} | {row['wave']} | {row['xp']} | "
              f"{row['core_points']}/{row['doctrine_points']} | {row.get('objective', row['map'])}")
        start = index
    print("Final equipped items:", json.dumps(last.get("equipped", []), ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", type=Path, help="A JSONL recording or an isolated run directory")
    args = parser.parse_args()
    paths = sorted(args.path.rglob("*.jsonl")) if args.path.is_dir() else [args.path]
    if not paths:
        parser.error("No JSONL recordings found")
    for path in paths:
        report(path)


if __name__ == "__main__":
    main()
