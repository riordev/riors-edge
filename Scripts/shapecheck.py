"""Did the fix reach the shape's other instances, or only the one that was reported?

WHY THIS AND NOT A CORPUS SCAN. The obvious instrument is a block-aware sweep of
every comment in the tree looking for justifications that outlived their cause.
That was the plan, and it is the wrong instrument for the defect that actually
keeps happening. Four in one week were NOT stale justifications sitting in a
corner:

    a tautological assertion deleted, its twin four lines below left standing
    a transcription guard checking form, accepting the wrong operands
    an arithmetic band centre corrected, the identical one a line down untouched
    a predicate widened for loot, the same predicate left narrow for ammo

Every one is a CORRECT FIX APPLIED TO ONE INSTANCE OF A REPEATED SHAPE. A corpus
scan catches none of them, because at the moment of the commit the corpus is
exactly as clean as it was before -- minus one. The signal is not in the tree, it
is in the DIFF: something was just identified as wrong, and the question nobody
asks is where else it lives.

So this runs on a commit, not on the codebase. For each shape it recognises in
the deleted lines, it searches the tree for surviving instances and reports the
ones the commit did not go near -- by LINE RANGE, not by file, because a
neighbour in the same file is exactly the case that keeps being missed.

WHAT IT CANNOT DO, stated because a checker that overstates itself is worse than
none. It recognises the shapes in SHAPES below and nothing else; a defect whose
shape is not listed produces silence, and silence here means "not looked for",
never "not present". It matches text, so it cannot tell a real instance from a
comment about one. And it reports SITES, not verdicts -- a listed sibling may be
perfectly correct, and the deliverable is that each was looked at and said to be.

Usage:
    python Scripts/shapecheck.py [<rev>]      # default HEAD
"""

import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "Source", "RiorsEdge")


# Each shape: what a fix for it looks like when DELETED, and how to find the
# other instances. `sibling` runs over every production source line.
SHAPES = [
    {
        "key": "self-referential-assertion",
        "what": "an assertion comparing a value against its own definition",
        "deleted": re.compile(r"Test(Equal|True)\s*\(.*(RewriteLayerCeiling|MaximumMinorStackStep|"
                              r"MinorStackStep|LayerCeiling)", re.I),
        "sibling": re.compile(r"Test(Equal|True)\s*\("),
        "narrow": True,
        "note": "Only assertions in the same file are listed: an assertion is a tautology "
                "relative to the definitions around it, so a cross-file list would be noise.",
    },
    {
        "key": "arithmetic-centre-of-a-multiplicative-band",
        "what": "a band centre written as an arithmetic mean where the band composes multiplicatively",
        "deleted": re.compile(r"BandMid\s*=|BandMid\b"),
        "sibling": re.compile(r"constexpr\s+float\s+\w*BandMid\w*\s*="),
        "narrow": False,
        "note": "Every *BandMid in the tree. Two exist and both are arithmetic means.",
    },
    {
        "key": "narrow-rank-predicate",
        "what": "a predicate testing one enum value where the enum is ordered and higher values qualify",
        "deleted": re.compile(r"IsElite\(\)|==\s*EBreaker\w*Rank::"),
        "sibling": re.compile(r"IsElite\(\)|==\s*EBreaker\w*Rank::\w+"),
        "narrow": False,
        "note": "Ordered-enum equality tests. A reward or gameplay site asking == Elite is "
                "the shape; a telemetry site asking it is correct and should be said so.",
    },
    {
        "key": "absence-justification",
        "what": "a comment asserting a named thing does not exist, is inert, or cannot be authored",
        "deleted": re.compile(r"//.*(there is no|does not exist|no way to|cannot be authored|"
                              r"still the one inert|is still inert|has no consumer|nothing reads)", re.I),
        "sibling": re.compile(r"//.*(there is no |does not exist|no way to |cannot author|"
                              r"is still inert|the one inert|nothing reads |has no consumer)", re.I),
        "narrow": False,
        "note": "These are paragraphs, so a line match is a POINTER to a block a human reads, "
                "not a finding. Four of these were wrong this week and every one spanned "
                "more than one line.",
    },
]


def run(*args):
    return subprocess.run(args, cwd=ROOT, capture_output=True, text=True,
                          encoding="utf-8", errors="replace").stdout


def production_sources():
    for base, _, files in os.walk(SRC):
        if os.sep + "Tests" + os.sep in base + os.sep:
            test = True
        else:
            test = False
        for f in files:
            if f.endswith((".cpp", ".h")):
                yield os.path.relpath(os.path.join(base, f), ROOT).replace(os.sep, "/"), test


def main():
    rev = sys.argv[1] if len(sys.argv) > 1 else "HEAD"
    diff = run("git", "show", "--unified=0", "--format=", rev)
    if not diff.strip():
        print(f"shapecheck: {rev} has no diff to read.")
        return 0

    deleted = [l[1:] for l in diff.splitlines() if l.startswith("-") and not l.startswith("---")]

    # TOUCHED IS A SET OF LINE RANGES, NOT A SET OF FILES, and the first version
    # of this script got that wrong in the exact way it exists to catch. With
    # file granularity, a neighbour in the SAME FILE as the fix reads as
    # "checked" for free -- which is precisely the AtCapBandMid case: it sat one
    # line below the constant that was corrected, in the same file, and a
    # file-level check would have called it covered while it was untouched.
    #
    # An instrument that reports the defect it embodies is worse than no
    # instrument, so this reads the hunk headers.
    touched = {}
    current = None
    for line in diff.splitlines():
        if line.startswith("+++ b/"):
            current = line[6:]
            touched.setdefault(current, [])
        elif line.startswith("@@") and current:
            m = re.match(r"@@ -\d+(?:,(\d+))? \+(\d+)(?:,(\d+))? @@", line)
            if m:
                start = int(m.group(2))
                count = int(m.group(3) or 1)
                touched[current].append((start, start + max(count, 1) - 1))

    def was_checked(rel, line_no, slack=12):
        """Did the commit change anything within a dozen lines of this site?

        The slack is deliberate and it is a JUDGEMENT, not a measurement: a fix
        and the neighbour it should have carried are usually in the same
        paragraph of code. Too small and every same-block neighbour reports as
        missed; too large and a file-sized edit swallows everything. Twelve is
        about one comment block in this codebase.
        """
        for start, end in touched.get(rel, []):
            if start - slack <= line_no <= end + slack:
                return True
        return False

    print(f"shapecheck {rev}: {len(deleted)} deleted lines, {len(touched)} files touched")
    print()

    sources = list(production_sources())
    any_hit = False

    for shape in SHAPES:
        hits = [d for d in deleted if shape["deleted"].search(d)]
        if not hits:
            continue
        any_hit = True
        print(f"SHAPE: {shape['key']}")
        print(f"  corrected here: {shape['what']}")
        print(f"  {shape['note']}")

        siblings = []
        for rel, is_test in sources:
            if shape["narrow"] and rel not in touched:
                continue
            try:
                with open(os.path.join(ROOT, rel), "rb") as f:
                    text = f.read().decode("utf-8", "replace")
            except OSError:
                continue
            for n, line in enumerate(text.splitlines(), 1):
                if shape["sibling"].search(line):
                    siblings.append((rel, n, line.strip()[:96], is_test))

        untouched = [s for s in siblings if not was_checked(s[0], s[1])]
        print(f"  {len(siblings)} site(s) match the shape; {len(untouched)} the commit did not go near")
        for rel, n, line, is_test in untouched[:40]:
            print(f"    {'[test] ' if is_test else '       '}{rel}:{n}  {line}")
        if len(untouched) > 40:
            print(f"    ... and {len(untouched) - 40} more")
        print()

    if not any_hit:
        print("No recognised shape in this commit's deletions.")
        print("That is NOT a clean bill: it means none of the listed shapes matched.")
    print("Every site above needs a human verdict. Listing is not judging.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
