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


# Each shape has two roles and they are NOT the same question:
#
#   find    -- where does this shape live in the tree?
#   trigger -- did this commit just correct an instance of it?
#
# THE TRIGGER MUST NEVER BE NARROWER THAN THE FIND, and it was. absence-
# justification triggered on "cannot be authored" while finding "cannot author",
# so b4d5d37 -- one of the four cases in this file's own docstring -- removed the
# line "genuinely cannot author even as a placeholder" and the tool said nothing.
# Two patterns for one shape, drifted apart, which is the same defect family the
# tool exists to catch.
#
# So `trigger` now DEFAULTS to `find`, and is only given separately where the two
# questions genuinely differ -- which is once, and the reason is written there.
SHAPES = [
    {
        "key": "self-referential-assertion",
        "what": "an assertion comparing a value against its own definition",
        # THE ONE PLACE THE TWO QUESTIONS DIFFER. Finding every assertion in the
        # tree is useless -- a tautology is only visible against the definitions
        # around it -- so the find is any assertion in a file the commit already
        # touched, while the trigger names the constants whose tautologies have
        # actually been found. A new one is added here when it is found, not
        # guessed at in advance.
        "trigger": re.compile(r"Test(Equal|True)\s*\(.*(RewriteLayerCeiling|MaximumMinorStackStep|"
                              r"MinorStackStep|LayerCeiling)", re.I),
        "find": re.compile(r"Test(Equal|True)\s*\("),
        "narrow": True,
        "note": "Only assertions in files this commit touched: a tautology is relative to the "
                "definitions around it, so a tree-wide list would be noise.",
    },
    {
        "key": "arithmetic-centre-of-a-multiplicative-band",
        "what": "a band centre written as an arithmetic mean where the band composes multiplicatively",
        "find": re.compile(r"constexpr\s+float\s+\w*BandMid\w*\s*=|\bBandMid\b"),
        "narrow": False,
        "note": "Every *BandMid in the tree. Two exist and both are arithmetic means.",
    },
    {
        "key": "narrow-rank-predicate",
        "what": "a predicate testing one enum value where the enum is ordered and higher values qualify",
        "find": re.compile(r"IsElite\(\)|==\s*EBreaker\w*Rank::\w+"),
        "narrow": False,
        "note": "Ordered-enum equality tests. A reward or behaviour site asking == Elite is the "
                "shape; a telemetry site asking it is correct, and the two live survivors of "
                "the first pass were both BEHAVIOUR sites missed because the fix was scoped by "
                "category rather than by shape.",
    },
    {
        "key": "value-at-its-own-clamp-bound",
        "what": "a magnitude authored at the edge of its own clamp, so the scale is saturated "
                "and no bonus applied to it can move",
        "find": re.compile(r"=\s*1\.0f\s*;.*ClampMax\s*=\s*\"1|ClampMax\s*=\s*\"1\".*=\s*1\.0f"),
        "narrow": False,
        "note": "BossDropChance = 1.0f under ClampMax=\"1\" made the Drop Chance affix inert on "
                "a boss and capped the whole axis at a 10x spread. A value sitting on its own "
                "bound is a scale that has run out, not a number that was chosen.",
    },
    {
        "key": "absence-justification",
        "what": "a comment asserting a named thing does not exist, is inert, or cannot be authored",
        "find": re.compile(r"//.*(there is no |does not exist|no way to |cannot author|"
                           r"cannot be authored|is still inert|the one inert|nothing reads |"
                           r"has no consumer|no consumer)", re.I),
        "narrow": False,
        "comments_are_instances": True,
        "note": "These are paragraphs, so a line match is a POINTER to a block a human reads, "
                "not a finding. Four of these were wrong this week and every one spanned "
                "more than one line. Expect a large number here: it is a reading list, and "
                "the floor on it is 17.",
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

    # THE WHOLE HUNK, NOT JUST DELETIONS -- and reading only deletions made this
    # tool miss two of the four cases in its own docstring.
    #
    # b4d5d37, "Drop Chance stops lying on a boss", reported nothing at all: the
    # correction was a new test and a new comment, so there was no `-` line
    # carrying the shape. And the band-mid catch on 288c911 fired by COINCIDENCE
    # -- on a deleted tautology line that happened to contain the text
    # EndgameBandMid, not on the mid correction, which was purely additive. A
    # commit that adds the note and deletes nothing reports clean.
    #
    # Corrections land on the `+` side constantly: a comment recording a
    # finding, a new guard, a call swapped for a wider sibling. Noise rises, and
    # that trade is already accepted everywhere else here by listing sites
    # instead of judging them.
    changed = [l[1:] for l in diff.splitlines()
               if (l.startswith("-") and not l.startswith("---"))
               or (l.startswith("+") and not l.startswith("+++"))]

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

    print(f"shapecheck {rev}: {len(changed)} changed lines, {len(touched)} files touched")
    print()

    sources = list(production_sources())
    any_hit = False

    for shape in SHAPES:
        trigger = shape.get("trigger", shape["find"])
        hits = [d for d in changed if trigger.search(d)]
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
                if shape["find"].search(line):
                    siblings.append((rel, n, line.strip()[:96], is_test))

        untouched = [s for s in siblings if not was_checked(s[0], s[1])]

        # A RAW COUNT IS UNREADABLE AND WORSE THAN NONE. narrow-rank-predicate
        # returned 23 of 23, and the 23 included the predicate's OWN
        # DEFINITION, the header comments explaining the fix, and the test
        # documenting it. Trust that number and a reader sees 23 unexamined
        # defects; distrust it and the two real ones stay buried under it.
        #
        # The discriminator was already written in each shape's note ("a
        # telemetry site asking it is correct") and was doing nothing there, so
        # it moves into the output. Crude on purpose: comment, test and
        # self-definition are mechanically separable and everything else is
        # LIVE CODE A HUMAN MUST READ. It does not judge those -- a live site
        # may be perfectly correct -- it only stops them being counted
        # alongside their own documentation.
        # WHETHER A COMMENT IS AN INSTANCE DEPENDS ON THE SHAPE, and treating
        # that as universal nullified a whole shape. For narrow-rank-predicate a
        # `//` mentioning IsElite is documentation ABOUT the shape. For
        # absence-justification the comment IS the shape -- so filing all 141
        # matches as "self-documenting, not defects" withheld every real one and
        # reported the exact opposite of the finding.
        comments_are_instances = shape.get("comments_are_instances", False)

        def bucket(rel, line, is_test):
            stripped = line.lstrip()
            if not comments_are_instances and (
                    stripped.startswith("//") or stripped.startswith("*") or stripped.startswith("/*")):
                return "comment"
            if is_test:
                return "test"
            if re.search(r"\b(bool|float|int32|void)\s+\w+\s*\(.*\)\s*(const)?\s*\{", line):
                return "definition"
            return "live"

        graded = [(rel, n, line, bucket(rel, line, is_test)) for rel, n, line, is_test in untouched]
        live = [g for g in graded if g[3] == "live"]
        others = [g for g in graded if g[3] != "live"]
        counts = {}
        for g in others:
            counts[g[3]] = counts.get(g[3], 0) + 1
        tail = ", ".join(f"{v} {k}" for k, v in sorted(counts.items()))
        print(f"  {len(siblings)} site(s) match; {len(untouched)} the commit did not go near")
        print(f"  OF THOSE: {len(live)} live code" + (f"; {tail} (self-documenting, not defects)" if tail else ""))
        for rel, n, line, _ in live[:40]:
            print(f"    {rel}:{n}  {line}")
        if len(live) > 40:
            print(f"    ... and {len(live) - 40} more live sites")
        if others:
            print(f"    ({len(others)} comment/test/definition site(s) withheld — they describe the shape, "
                  "they are not instances of it)")
        print()

    if not any_hit:
        print("No recognised shape in this commit's changed lines.")
        print("That is NOT a clean bill: it means none of the listed shapes matched.")
    print("Every site above needs a human verdict. Listing is not judging.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
