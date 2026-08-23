#!/usr/bin/env python3
"""Generate Docs/STATE.md — the project's build state, measured rather than written.

Docs state intent. This states what is actually true, and the two must never be
kept in sync by hand: that is what produced a 37,000-line corpus.

HOW TO READ A SECTION
---------------------
Every section declares a DIRECTION, and the direction is not the same for all
of them. Getting this wrong is worse than not reporting at all:

  CEILING  the number falls and never rises. Dead content, in all its forms.
  FLOOR    the number rises and never falls. Tree density: a ceiling here would
           lock the Core tree at its current size and report green while doing
           it.
  BAND     the number stays between two values. Balance targets.

A section with no pin is UNPINNED and reports its measurement without judging
it. Pins live in Scripts/status-pins.json and are authored deliberately.

WHY PINS ARE NOT AUTO-GENERATED FROM THE FIRST RUN
--------------------------------------------------
Because for several sections the current state IS the problem. Auto-pinning
would enshrine today's dysfunction as the standard, which is the exact failure
the ratchet exists to prevent. Where the current state is acceptable and
shrinking, the pin is the measurement. Where the current state is the problem,
the pin is the TARGET and the section is expected to sit outside it until the
work lands. A human decides which is which, once, per section.

THE INVARIANT TABLES ARE A MACHINE-READABLE SURFACE, AND ONLY BY CONVENTION
---------------------------------------------------------------------------
Each file in Docs/spec/ carries exactly one "## Asserted invariants" section
containing a two-column markdown table whose right column is a test name. That
shape is load-bearing for the unimplemented-invariants section below.

If a spec's table stops matching, this script FAILS LOUDLY rather than
reporting zero unimplemented invariants. A parse that silently finds nothing is
indistinguishable from a project with nothing wrong, and this project has
shipped that confusion four times in other forms.
"""

import json
import os
import re
import subprocess
import sys
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "Source", "RiorsEdge")
SPEC = os.path.join(ROOT, "Docs", "spec")
PINS = os.path.join(ROOT, "Scripts", "status-pins.json")
OUT = os.path.join(ROOT, "Docs", "STATE.md")
# THE SUITE GETS ITS OWN FILE, and this is PREVENTION rather than detection.
# riors_edge.log is the project's default log name, so the interactive editor, a
# standalone run and the capture harness all open it and rotate whatever was
# there into a backup. Opening the editor therefore DESTROYED the suite record,
# and this report then read the editor's log and found no tests in it. The suite
# is run with -abslog pointing here, so the collision cannot happen at all.
LOG = os.path.join(ROOT, "Saved", "Logs", "suite.log")

CEILING, FLOOR, BAND = "ceiling", "floor", "band"


class ParseError(Exception):
    """A source or spec surface stopped matching its expected shape."""


# --------------------------------------------------------------------------
# Source reading
# --------------------------------------------------------------------------

def read(path):
    with open(path, "rb") as f:
        return f.read().decode("utf-8", "surrogateescape")


def source_files(exts=(".cpp", ".h")):
    for base, _, files in os.walk(SRC):
        for f in files:
            if f.endswith(exts):
                yield os.path.join(base, f)


def load_sources():
    return {p: read(p) for p in source_files()}


def non_test_sources(sources, exclude_substrings=()):
    out = {}
    for path, text in sources.items():
        rel = os.path.relpath(path, SRC).replace("\\", "/")
        if rel.startswith("Tests/"):
            continue
        if any(s in rel for s in exclude_substrings):
            continue
        out[rel] = text
    return out


# --------------------------------------------------------------------------
# Progression parsing — nodes, effects, tags, trees
# --------------------------------------------------------------------------

LIB = "Progression/BreakerProgressionLibrary.cpp"
TYPES = "Progression/BreakerProgressionTypes.h"

MAKENODE = re.compile(
    r'MakeNode\(\s*TEXT\("([^"]+)"\)\s*,\s*TEXT\("[^"]*"\)\s*,\s*'
    r'TEXT\("(?:[^"\\]|\\.)*"\)\s*,\s*[^,]+,\s*[^,]+,\s*'
    r'(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)',
    re.S,
)
TREEFN = re.compile(r'UBreakerProgressionTree\* UBreakerProgressionLibrary::(Get\w+)\(\)')


def parse_nodes(lib_text):
    """Every authored node, with its tree, tier, rank count, cost, effects and tags."""
    lines = lib_text.split("\n")
    bounds = [(i, m.group(1)) for i, l in enumerate(lines)
              for m in [TREEFN.match(l.strip())] if m]
    if not bounds:
        raise ParseError(f"{LIB}: no tree functions matched. The authoring shape changed.")
    bounds.append((len(lines), "<end>"))

    def tree_of(i):
        for k in range(len(bounds) - 1):
            if bounds[k][0] <= i < bounds[k + 1][0]:
                return bounds[k][1]
        return "<preamble>"

    # A MakeNode call spans several lines, so match against the whole text and
    # map the match offset back to a line. Matching per-line finds nothing,
    # which is exactly the silent-zero this parser refuses to produce.
    starts = [0]
    for l in lines:
        starts.append(starts[-1] + len(l) + 1)

    def line_of(offset):
        lo, hi = 0, len(starts) - 1
        while lo < hi - 1:
            mid = (lo + hi) // 2
            if starts[mid] <= offset:
                lo = mid
            else:
                hi = mid
        return lo

    nodes = []
    for m in MAKENODE.finditer(lib_text):
        i = line_of(m.start())
        node = {
            "id": m.group(1), "tier": int(m.group(2)),
            "ranks": int(m.group(3)), "cost": int(m.group(4)),
            "tree": tree_of(i), "line": i + 1,
            "effects": [], "tags": [], "cornerstone": False,
            "conditions": [], "more": False,
        }
        # Walk forward to this node's Tree->Nodes.Add, collecting what it authors.
        for j in range(i + 1, min(i + 60, len(lines))):
            l = lines[j]
            if "Tree->Nodes.Add" in l:
                break
            for e in re.finditer(r'AddEffect\([^,]+,\s*EBreakerNodeStatTarget::(\w+)', l):
                node["effects"].append(e.group(1))
            for e in re.finditer(r'AddMoreEffect\(', l):
                node["effects"].append("Damage")
                node["more"] = True
            if "MorePercent" in l:
                node["more"] = True
            for e in re.finditer(r'EBreakerBuildCondition::(\w+)', l):
                if e.group(1) != "Always":
                    node["conditions"].append(e.group(1))
            for t in re.finditer(r'BreakerNodeTags::(\w+)', l):
                node["tags"].append(t.group(1))
            if "bCornerstone = true" in l:
                node["cornerstone"] = True
        nodes.append(node)

    if len(nodes) < 100:
        raise ParseError(
            f"{LIB}: parsed only {len(nodes)} nodes. The MakeNode signature changed; "
            "fix this parser rather than trusting the number.")
    return nodes


def parse_lane_register(types_text):
    """Stat targets the aggregator actually pays, and every target that exists."""
    all_targets = re.search(
        r'enum class EBreakerNodeStatTarget\s*:\s*uint8\s*\{(.*?)\}\s*;',
        types_text, re.S)
    if not all_targets:
        raise ParseError(f"{TYPES}: EBreakerNodeStatTarget enum not found.")
    targets = [t.strip() for t in re.findall(r'^\s*(\w+)', all_targets.group(1), re.M)]
    targets = [t for t in targets if t not in ("Count",) and not t.startswith("//")]

    fn = re.search(
        r'BreakerStatTargetHasAggregationLane\(EBreakerNodeStatTarget Target\)\s*\{(.*?)\n\}',
        types_text, re.S)
    if not fn:
        raise ParseError(f"{TYPES}: BreakerStatTargetHasAggregationLane not found.")
    body = fn.group(1)
    paid = set()
    for m in re.finditer(r'case EBreakerNodeStatTarget::(\w+):', body):
        # Cases fall through to a single `return true;` block; take everything
        # before the first `return false` as paid.
        paid.add(m.group(1))
    false_at = body.find("return false")
    if false_at != -1:
        paid = {m.group(1) for m in
                re.finditer(r'case EBreakerNodeStatTarget::(\w+):', body[:false_at])}
    if not paid:
        raise ParseError(f"{TYPES}: lane register parsed as empty.")

    # Targets that are correctly laneless BY RULE, and so are not waiting for
    # anything. Parsed from the same header rather than listed here, for the
    # reason the lane register itself is hand-maintained in the source: a list
    # in this file would be a second place to update and the first one to rot.
    rider = set()
    rider_fn = re.search(
        r'BreakerStatTargetIsRiderDelivered\(EBreakerNodeStatTarget Target\)\s*\{(.*?)\n\}',
        types_text, re.S)
    if rider_fn:
        rider = {m.group(1) for m in
                 re.finditer(r'EBreakerNodeStatTarget::(\w+)', rider_fn.group(1))}
    return targets, paid, rider


def parse_conditions(text):
    m = re.search(r'enum class EBreakerBuildCondition\s*:\s*uint8\s*\{(.*?)\}\s*;', text, re.S)
    if not m:
        raise ParseError("EBreakerBuildCondition enum not found.")
    out = [c.strip() for c in re.findall(r'^\s*(\w+)', m.group(1), re.M)]
    return [c for c in out if c not in ("Count",)]


def parse_declared_tags(lib_text):
    """{identifier: "Progression.Node...."} for every declared node tag.

    THE STRING IS HALF THE ANSWER. A consumer may name the identifier, or it may
    request the tag by its full string through an accessor --
    BreakerOverpenetrationTag() is
    RequestGameplayTag("Progression.Node.Swift.Marksman.Overpenetration"), and
    nothing in that file mentions Node_Overpenetration at all. Matching only the
    identifier reported two genuinely-consumed tags as dead.
    """
    return dict(re.findall(r'UE_DEFINE_GAMEPLAY_TAG\((\w+),\s*"([^"]+)"', lib_text))


# --------------------------------------------------------------------------
# Consumption — BOTH axes
# --------------------------------------------------------------------------
# A node is live if its tag has a consumer OR its node id does. The project
# reads both: HasNodeTag(BreakerNodeTags::Node_X) and
# GetClassNodeRank(BreakerSteadyNodeId). A report that greps only tags calls
# live nodes dead, which is how a previous audit reached "86 of 94 tags have no
# consumer" and overstated the problem.

def strip_cpp_comments(text):
    """C++ source with comments removed and string literals kept.

    THE INDEX MUST NOT READ PROSE. Consumer detection is a text search over
    production sources, and it was searching comments too -- so a tag named in a
    WAITING-ON roster counted as consumed, and a node whose payload nothing
    reads reported as live. Six independent per-tree audits put the real silent
    count higher than this report did, in the optimistic direction, which is the
    worst direction for a number nobody re-derives.

    String literals are preserved deliberately: id_consumed matches quoted node
    ids, and the tag search below matches the full "Progression.Node..." string
    an accessor requests. Stripping those would trade one blind spot for
    another. Comments become a single space so two tokens either side of one
    cannot merge into a third that matches nothing.
    """
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == '"' or c == "'":
            quote = c
            out.append(c)
            i += 1
            while i < n:
                out.append(text[i])
                if text[i] == '\\' and i + 1 < n:
                    out.append(text[i + 1])
                    i += 2
                    continue
                if text[i] == quote:
                    i += 1
                    break
                i += 1
            continue
        if c == '/' and i + 1 < n and text[i + 1] == '/':
            while i < n and text[i] != '\n':
                i += 1
            out.append(' ')
            continue
        if c == '/' and i + 1 < n and text[i + 1] == '*':
            i += 2
            while i + 1 < n and not (text[i] == '*' and text[i + 1] == '/'):
                i += 1
            i += 2
            out.append(' ')
            continue
        out.append(c)
        i += 1
    return ''.join(out)


def stripped_sources(sources, **kwargs):
    """non_test_sources with every comment removed. See strip_cpp_comments."""
    return {rel: strip_cpp_comments(text)
            for rel, text in non_test_sources(sources, **kwargs).items()}


def build_consumer_index(sources):
    """The text every "does anything read this?" question is answered against.

    WHAT IT CAN SEE: production source outside the node library, comments
    stripped, string literals kept. So a tag reaches it as the identifier
    (BreakerNodeTags::Node_X) or as the full "Progression.Node..." string an
    accessor requests, and a node id reaches it quoted or through a
    Breaker<Name>NodeId constant.

    WHAT IT CANNOT SEE, and these are the ways it still says yes when the answer
    is no: a read that is compiled out, a read behind a branch nothing takes, a
    tag mentioned in a disabled block, and any consumer that lives in a Data
    Asset rather than in source. It is a text search, not a call graph. Treat
    every number it feeds as an UPPER bound on how much is alive.

    It also cannot see the library itself, deliberately: the file that DECLARES
    every tag would otherwise consume all of them.
    """
    prod = stripped_sources(sources, exclude_substrings=("Progression/BreakerProgressionLibrary",))
    return "\n".join(prod.values())


def tag_consumed(index, tag, tag_string=None):
    """Either spelling counts: the identifier, or the full tag string."""
    if re.search(r'\b' + re.escape(tag) + r'\b', index):
        return True
    return bool(tag_string) and ('"' + tag_string + '"') in index


def id_consumed(index, node_id):
    if re.search(r'"' + re.escape(node_id) + r'"', index):
        return True
    short = node_id.split(".")[-1]
    return re.search(r'\bBreaker' + re.escape(short) + r'NodeId\b', index) is not None


# --------------------------------------------------------------------------
# Spec invariant tables
# --------------------------------------------------------------------------

def parse_spec_invariants():
    """Every (spec, invariant, test name) the specs assert.

    Fails loudly rather than returning an empty list: a spec whose table stopped
    matching looks exactly like a spec with nothing to assert.
    """
    if not os.path.isdir(SPEC):
        raise ParseError(f"{SPEC} does not exist.")
    specs = sorted(f for f in os.listdir(SPEC) if f.endswith(".md"))
    if not specs:
        raise ParseError(f"{SPEC} contains no specs.")

    out = []
    for name in specs:
        text = read(os.path.join(SPEC, name))
        section = re.search(r'^## Asserted invariants\s*$(.*?)(?=^## |\Z)', text, re.S | re.M)
        if not section:
            raise ParseError(
                f"{name}: no '## Asserted invariants' section. Every spec carries one; "
                "if this spec genuinely asserts nothing, say so in an empty table "
                "rather than removing the heading.")
        rows = re.findall(r'^\|(?!\s*-)([^|\n]+)\|([^|\n]+)\|\s*$', section.group(1), re.M)
        rows = [(a.strip(), b.strip()) for a, b in rows
                if a.strip().lower() != "invariant" and set(a.strip()) != {"-"}]
        if not rows:
            raise ParseError(
                f"{name}: 'Asserted invariants' section parsed to zero rows. The table "
                "shape changed — fix this parser or the table, but do not let it "
                "report zero.")
        for invariant, test in rows:
            for t in re.findall(r'`([^`]+)`', test) or [test]:
                out.append((name, invariant, t.strip()))
    return out


# A // comment to end-of-line, inside a macro head. Built from chr(10) so the
# pattern survives being edited through a shell heredoc, which silently turned
# the escape into a real newline once already.
COMMENT_RE = '//[^' + chr(10) + ']*'


def parse_declared_tests(sources):
    """Every automation test name the source tree declares.

    THIS SET IS THE REFERENCE the suite log is reconciled against, so a name it
    cannot see is a test that can vanish from a run unnoticed. The old pattern
    required the name to follow the class directly, and one macro carries a
    comment between the two — RiorsEdge.Progression.RuleBandImpact, which has a
    passing test and was reported as an UNIMPLEMENTED INVARIANT the whole time,
    because this parse is also what the invariant section matches against. The
    macro head is stripped of comments before the name is read.
    """
    names = set()
    for path, text in sources.items():
        if "Tests" not in path:
            continue
        for m in re.finditer(
                r'IMPLEMENT_\w*AUTOMATION_TEST\s*\((.*?)\)\s*$', text, re.S | re.M):
            head = re.sub(COMMENT_RE, '', m.group(1))
            q = re.search(r'"([^"]+)"', head)
            if q:
                names.add(q.group(1))
    if not names:
        raise ParseError("No automation tests found. The IMPLEMENT_ macro shape changed.")
    return names


def normalise_test(name):
    return name[len("RiorsEdge."):] if name.startswith("RiorsEdge.") else name


# --------------------------------------------------------------------------
# Suite log — expected red, unexpected red
# --------------------------------------------------------------------------

def expected_red_names(entries):
    """An entry is `Test.Name :: why it is red and what deletes it`.

    The policy requires every deliberate red to carry the finding it encodes
    and its deletion condition, so the entry cannot be a bare test name — but
    the matcher needs one. The name is everything before the separator.
    """
    return {e.split("::", 1)[0].strip() for e in entries}


def parse_suite_log(expected_red, declared):
    """Read the suite log, and REFUSE a log that does not reconcile.

    THE COUNT MUST BALANCE, and this is here because it did not. Every result
    below is scraped from a `Test Completed` line, so a test that STARTS and
    never completes is invisible to this function, to the log grep in the
    working-rules file, and to anybody reading either — it is not counted as
    passing, not counted as failing, and not counted as missing. For a while the
    run quit with a forced exit that killed the process before the last test's
    completion line was flushed, so the alphabetically-final test was
    unverifiable by construction: had it been red, every report said clean.

    Fixing that race was one word. It is not the durable fix, because the next
    race will arrive in a different shape — a crash mid-test, a hang, a worker
    that drops a message — and all of them look identical from here. A count
    that RECONCILES cannot be beaten by any of them: started must equal
    completed, and a mismatch refuses the report rather than printing a number
    that is quietly short. That is also how this was found — a total that was
    one below what the test files contained.

    STARTED AGAINST COMPLETED IS STILL AN INTERNAL CHECK, and an internal check
    cannot see an EMPTY log: zero balances zero. A log clobbered by another run
    returned zero started, zero passed and zero unexpected red without raising,
    and the report printed "unexpected red: 0" -- the exact line the discipline
    reads -- off a file containing no suite at all. A count checked only against
    itself cannot tell you it is short.

    The outer check is against something the RUN does not get to author: the
    test names declared in the source tree. Every declared test must have
    started. That one comparison catches empty, clobbered, partial, filtered and
    killed runs, because each leaves a declared test with no start line.

    It is deliberately NOT a pinned minimum count. A pin would be a second copy
    of the passing total, hand-maintained, wrong the first time a test is added,
    and a number that only ever goes up cannot tell you it is short either.
    """
    if not os.path.isfile(LOG):
        return None
    expected_red = expected_red_names(expected_red)
    text = read(LOG)
    started = set(re.findall(r'Test Started\. Name=\{[^}]*\} Path=\{([^}]+)\}', text))
    passed = set(re.findall(r'Result=\{Success\} Name=\{[^}]*\} Path=\{([^}]+)\}', text))
    failed = set(re.findall(r'Result=\{Fail\} Name=\{[^}]*\} Path=\{([^}]+)\}', text))

    never_started = declared - started
    if never_started:
        shown = sorted(never_started)
        raise ParseError(
            "the suite log does not reconcile: %d declared test(s) never started - "
            % len(never_started) + ", ".join(shown[:12])
            + (" (and %d more)" % (len(shown) - 12) if len(shown) > 12 else "")
            + ". Every test the source tree declares must appear in the run. A log "
              "missing them is empty, clobbered by another run writing the same file, "
              "filtered to a subset, or killed part way, and all four report a total "
              "that is silently short. Re-run the suite with "
              "-abslog=<repo>/Saved/Logs/suite.log.")

    unfinished = started - passed - failed
    if unfinished:
        raise ParseError(
            "the suite log does not reconcile: %d test(s) started and never completed — "
            % len(unfinished) + ", ".join(sorted(unfinished))
            + ". A test with no result is counted nowhere, so every total in this report "
              "would be silently short. Re-run the suite; if it recurs, the run is being "
              "killed before it finishes rather than finishing.")

    exp = {t for t in failed if normalise_test(t) in expected_red}
    unexp = failed - exp
    missing_red = {t for t in expected_red if "RiorsEdge." + t in passed}
    return {"passed": passed, "expected_red": exp, "unexpected_red": unexp,
            "no_longer_red": missing_red, "started": started}


# --------------------------------------------------------------------------
# Sections
# --------------------------------------------------------------------------

def classify(node):
    """Node SHAPE, keyed on payload — never on what the node costs or how many
    times you buy it.

    The previous version read cornerstone / cost / ranks and nothing else, so it
    was measuring RANK PRICING and reporting it as shape. Two nodes with byte-
    identical rule payloads landed in different buckets because one was priced at
    two ranks and the other at one; a doctrine shape read 62% or 0% purely on
    that choice. A metric that answers a pricing question cannot be evidence in
    a shape argument, and 120 nodes were about to be authored against it.

    The signals here are all payload facts, in the spec's own vocabulary
    ("ranked minors, notables carrying a rule or a condition, and convergence
    and keystone"):

      convergence/keystone  authors a More, or is flagged a cornerstone. O3
                            permits a More ONLY on a convergence or keystone
                            node, so authoring one IS the signal.
      ranked minor          an unconditional stat line, and nothing else.
      notable               carries a condition, or carries no stat line at all
                            because its payload is a rule.

    A granted tag is deliberately NOT a notable signal on its own: this project
    authors rules-as-tags on nearly every node including stat nodes, so reading
    the tag alone put 91% of the tree in one bucket and said nothing.

    THE FOURTH BUCKET IS NOT COMPUTED HERE, and that is deliberate. Scaffolding
    — a gateway, a link, a node whose only payload is a tag nothing reads —
    cannot be told from a real rule node by payload alone: every no-stat node in
    this tree carries a tag, so a payload-only view finds no residual at all.
    Separating them needs the consumer index, which build_sections has and this
    function must not, or classify() stops being a pure statement about shape.
    The section computes it there and reports it on its own line.
    """
    if node["cornerstone"] or node["more"]:
        return "convergence/keystone"
    if node["effects"] and not node["conditions"]:
        return "ranked minor"
    if node["effects"] or node["conditions"]:
        return "notable"
    # No stat line and no condition: the payload is a rule, and a granted tag is
    # how this project delivers one. The tag speaks ONLY here — used earlier it
    # would swallow every stat node that also carries an identifying tag, which
    # is most of them.
    return "notable"


def build_sections(sources):
    lib = sources[os.path.join(SRC, *LIB.split("/"))]
    types = sources[os.path.join(SRC, *TYPES.split("/"))]
    conds_text = read(os.path.join(SRC, "Progression", "BreakerBuildConditions.h"))

    nodes = parse_nodes(lib)
    targets, paid, rider_delivered = parse_lane_register(types)
    conditions = parse_conditions(conds_text)
    declared_tags = parse_declared_tags(lib)
    index = build_consumer_index(sources)

    sections = []

    # --- silent nodes -----------------------------------------------------
    silent = []
    for n in nodes:
        pays = any(e in paid for e in n["effects"])
        heard = any(tag_consumed(index, t, declared_tags.get(t)) for t in n["tags"]) or id_consumed(index, n["id"])
        if not pays and not heard:
            silent.append(n)
    per_tree = defaultdict(int)
    for n in silent:
        per_tree[n["tree"]] += 1
    sections.append({
        "key": "silent-nodes", "title": "Silent nodes", "direction": CEILING,
        "value": len(silent), "unit": f"of {len(nodes)} authored",
        "detail": [f"{t}: {c}" for t, c in sorted(per_tree.items(), key=lambda x: -x[1])],
        "note": "Authored, purchasable, costs a point, and produces no observable change. "
                "Counted against BOTH consumption axes — tag and node id.",
    })

    # --- unmapped stat targets -------------------------------------------
    # RIDER-DELIVERED TARGETS ARE NOT UNMAPPED. They will never have a lane —
    # O98 rules melee a tag-keyed slice of the weapon pool, not a fourth pool —
    # so counting them here made a ratcheting ceiling that could not reach zero,
    # and three separate readings mistook one for a lane a content pass would
    # light up. They are reported on their own line instead.
    unmapped = [t for t in targets if t not in paid and t not in rider_delivered]
    authored_targets = {e for n in nodes for e in n["effects"]}
    # Iterate `targets` (an ordered list) rather than `paid` (a set): a report
    # whose lines reorder between runs shows a diff on every regeneration, and
    # a file that always shows a diff is one people stop reading diffs on.
    # Same reasoning as dropping the commit stamp — determinism is what makes
    # this file's diffs worth reading.
    empty_lanes = [t for t in targets if t in paid and t not in authored_targets]
    sections.append({
        "key": "unmapped-stat-targets", "title": "Stat targets with no aggregation lane",
        "direction": CEILING, "value": len(unmapped), "unit": f"of {len(targets)}",
        "detail": unmapped,
        "note": "A node authored against one of these is silently unpaid. "
                + (f"{len(rider_delivered)} further target(s) are delivered by a rider and are "
                   f"correctly laneless: {', '.join(sorted(rider_delivered))}." if rider_delivered else ""),
    })
    sections.append({
        "key": "empty-lanes", "title": "Aggregation lanes carrying nothing",
        "direction": CEILING, "value": len(empty_lanes), "unit": f"of {len(paid)} lanes",
        "detail": empty_lanes,
        "note": "Plumbing with no author. Not harmful, but not free either.",
    })

    # --- dead tags --------------------------------------------------------
    dead_tags = [t for t in declared_tags if not tag_consumed(index, t, declared_tags[t])]
    sections.append({
        "key": "dead-tags", "title": "Node tags with no consumer", "direction": CEILING,
        "value": len(dead_tags), "unit": f"of {len(declared_tags)} declared",
        "detail": dead_tags,
        "note": "A tag nothing reads. Some are legitimately reserved; most are a promise "
                "the game does not keep.",
    })

    # --- dead conditions --------------------------------------------------
    authored_conds = set()
    for m in re.finditer(r'EBreakerBuildCondition::(\w+)', lib):
        authored_conds.add(m.group(1))
    unused_conds = [c for c in conditions if c not in authored_conds and c != "Always"]
    sections.append({
        "key": "dead-conditions", "title": "Conditions no content authors",
        "direction": CEILING, "value": len(unused_conds), "unit": f"of {len(conditions)}",
        "detail": unused_conds,
        "note": "Vocabulary that exists and pays for nothing. Widening the vocabulary "
                "ahead of its consumers is what produced most of this list.",
    })

    # --- uncalled generation ---------------------------------------------
    # Comments stripped here for the same reason as the consumer index: a
    # Notify name mentioned in a comment is not a caller.
    notifies = []
    stripped = stripped_sources(sources)
    for rel, text in stripped.items():
        if not rel.startswith("Classes/") or not rel.endswith(".h"):
            continue
        for m in re.finditer(r'\bvoid (Notify\w+)\s*\(', text):
            notifies.append((rel, m.group(1)))
    uncalled = []
    for rel, fn in notifies:
        callers = [r for r, t in stripped.items()
                   if r != rel and re.search(r'\b' + fn + r'\s*\(', t)]
        if not callers:
            uncalled.append(f"{rel}::{fn}")
    sections.append({
        "key": "uncalled-generation", "title": "Resource generation entry points with no caller",
        "direction": CEILING, "value": len(uncalled), "unit": f"of {len(notifies)}",
        "detail": uncalled,
        "note": "A generation hook nothing calls is a resource bar that sits at zero forever.",
    })

    # --- unimplemented invariants ----------------------------------------
    asserted = parse_spec_invariants()
    declared = parse_declared_tests(sources)
    declared_norm = {normalise_test(d) for d in declared}
    unimplemented = []
    for spec, invariant, test in asserted:
        base = normalise_test(test).rstrip(".*")
        hit = any(d == base or d.startswith(base + ".") for d in declared_norm)
        if not hit:
            unimplemented.append(f"{test}  —  {spec}")
    sections.append({
        "key": "unimplemented-invariants", "title": "Asserted invariants with no test",
        "direction": CEILING, "value": len(unimplemented),
        "unit": f"of {len(asserted)} asserted across {len(set(a[0] for a in asserted))} specs",
        "detail": unimplemented,
        "note": "A named test that was never written looks asserted and is not. This is "
                "worse than a red test, and it is the reason this section exists.",
    })

    # --- tree density: offered-to-spendable (FLOOR) -----------------------
    core_budget, class_budget = 65, 30
    by_tree = defaultdict(list)
    for n in nodes:
        by_tree[n["tree"]].append(n)
    ratios = []
    for tree, ns in sorted(by_tree.items()):
        offered = sum(n["cost"] * max(1, n["ranks"]) for n in ns)
        budget = core_budget if "Core" in tree else class_budget
        ratios.append((tree, len(ns), offered, round(offered / budget, 2)))
    worst = min((r[3] for r in ratios), default=0)
    sections.append({
        "key": "offered-to-spendable", "title": "Offered-to-spendable ratio, per tree",
        "direction": FLOOR, "value": worst, "unit": "worst tree",
        "detail": [f"{t}: {n} nodes, {o} points offered, {r}x budget"
                   for t, n, o, r in ratios],
        "note": "Most of a build should be refusal. A CEILING here would lock the trees "
                "at their current size and report green while doing it.",
    })

    # --- tree density: composition (BAND) ---------------------------------
    comp_rows = []
    for tree, ns in sorted(by_tree.items()):
        c = defaultdict(int)
        for n in ns:
            c[classify(n)] += 1
        total = max(1, len(ns))
        comp_rows.append("{}: {}% ranked minor, {}% notable, {}% convergence/keystone".format(
            tree,
            round(100 * c["ranked minor"] / total),
            round(100 * c["notable"] / total),
            round(100 * c["convergence/keystone"] / total)))
    all_minor = sum(1 for n in nodes if classify(n) == "ranked minor")
    # Scaffolding: no stat line, no condition, and no rule anything reads. A
    # STRICT SUBSET of the silent nodes above — the difference is the silent
    # nodes that ARE shaped and merely point at an unpaid target, which is a
    # wiring problem where this is an authoring one.
    all_nopay = sum(1 for n in nodes
                    if not n["effects"] and not n["conditions"]
                    and not any(tag_consumed(index, t, declared_tags.get(t)) for t in n["tags"])
                    and not id_consumed(index, n["id"]))
    sections.append({
        "key": "node-shape-composition", "title": "Node-shape composition, per tree",
        "direction": BAND, "value": round(100 * all_minor / max(1, len(nodes))),
        "unit": "% ranked minors, all trees",
        "detail": comp_rows,
        "note": "A tree that is almost entirely notable-shaped has nothing to fill a "
                "constellation with between the interesting picks. "
                f"Of these, {all_nopay} are SCAFFOLDING — no stat line, no condition, "
                "and no rule anything reads — a strict subset of the silent nodes "
                "above, where the remainder are shaped and merely unpaid. UNPINNED pending a re-derived band: "
                "60% ranked minors means 60% unconditional stat lines, and O76 gives "
                "raw percentages to affixes outright, so the authored 55-65 target "
                "cannot be reached without breaking another rule. Until it is "
                "re-derived this section reports and judges nothing.",
    })

    # --- scaffolding, pinned on its own -----------------------------------
    # SPLIT OUT OF THE NOTE ABOVE because O112 unpinned the composition BAND and
    # took this number down with it -- a count that judges nothing is a count
    # nobody defends. The band is genuinely unresolved; the scaffolding count is
    # not, and it is the one Phase 4 writes a test against. A node with no stat
    # line, no condition and no rule anything reads is an authoring failure at
    # any composition target.
    sections.append({
        "key": "scaffolding-nodes", "title": "Scaffolding nodes",
        "direction": CEILING, "value": all_nopay, "unit": f"of {len(nodes)} authored",
        "detail": [],
        "note": "No stat line, no condition, and no rule anything reads. A STRICT "
                "SUBSET of the silent nodes: the difference is the silent nodes that "
                "ARE shaped and merely point at an unpaid target, which is a wiring "
                "problem where this is an authoring one.",
    })

    return sections, asserted


# --------------------------------------------------------------------------
# Sections that need a test to emit a number
# --------------------------------------------------------------------------
# These are computed inside the suite, by code this script cannot reach without
# reimplementing it — and a second implementation of the drop pipeline or the
# aggregator is precisely the drift the single-source rule exists to prevent.
# Each names the test that owns the number. Until that test logs it in a form
# this script can read, the section reports honestly that it has no number
# rather than inventing one.

EMITTED_BY_TEST = [
    ("loot-per-hour", "Items dropped per hour, at the reference area level", BAND,
     "RiorsEdge.Items.Drops.LootPerHour"),
    ("power-band-atcap", "Build variance band, at cap", BAND,
     "RiorsEdge.Progression.PowerBand.AtCap"),
    ("power-band-endgame", "Build variance band, endgame", BAND,
     "RiorsEdge.Progression.PowerBand.Endgame"),
    # PARITY, not a variance band: what an ability-geared build's ability lane
    # composes to against a weapon-geared build's weapon lane at the cap. That
    # is the figure power-and-scaling asserts ("ability throughput sits within
    # the parity band of weapon throughput at level 50") and the one the ~4%
    # measurement that motivated the three-pool split was expressed in. The
    # section title said "build variance band" before the test existed, which
    # was a guess about what would be measured; this is what is.
    ("power-band-ability", "Ability lane throughput against weapon lane, at cap", BAND,
     "RiorsEdge.Progression.PowerBand.AbilityLane"),
    # The same measurement at the top of the item-level ladder, reported beside
    # the cap figure and deliberately unpinned. O99 rules the band AT THE CAP;
    # whether it holds at item level 120 is a different question, because the
    # endgame band is far more crit-driven and crit is currently a weapon-lane
    # story. Divergence between these two rows is a finding of its own, and a
    # row that reports without judging is how the report says so.
    ("power-band-ability-endgame", "Ability lane throughput against weapon lane, endgame", BAND,
     "RiorsEdge.Progression.PowerBand.AbilityLane"),
    # A ceiling, not a band: a rewrite worth too little is a design problem to
    # notice, not a build to stop shipping. The test already asserts separately
    # that no rewrite LOWERS a build's damage, which is the lower edge.
    ("damage-vs-defence-growth", "Monster damage growth against gear defence growth", CEILING,
     "RiorsEdge.Combat.Chassis.DamageBelowHealth"),
    # A ceiling, not a band: a rewrite worth too little is a design problem to
    # notice, not a build to stop shipping. The test already asserts separately
    # that no rewrite LOWERS a build's damage, which is the lower edge.
    ("rewrite-impact", "Worst single rewrite step on an optimized build", CEILING,
     "RiorsEdge.Progression.RuleBandImpact"),
    # THE CAMPAIGN'S PAYOUT GAP, as a number. O7 rules fifteen world Core Points
    # canon and eight of the twenty-eight authored missions pay one as their
    # entire reward, so a source whose trigger does not exist is a mission with
    # nothing to give. A ceiling driving to zero: it can only fall, and it falls
    # by wiring a trigger rather than by editing this file.
    ("world-points-unwired", "World Core Point sources with no trigger", CEILING,
     "RiorsEdge.Progression.WorldPoints.SoloReachable"),
]


def parse_emitted(section_keys):
    """Read the [BreakerStatus] lines the suite emitted.

    These numbers are computed inside the suite by code this script cannot
    reach — the aggregator, the drop pipeline — and reimplementing either here
    would be a second source of truth for numbers whose whole value is that
    there is one. So the suite emits and this reads.

    A key that matches no section is REPORTED, not dropped. A status line
    nobody reads is the same silent nothing as a pin naming no section.
    """
    if not os.path.isfile(LOG):
        return {}, []
    found, unknown = {}, []
    for m in re.finditer(r'\[BreakerStatus\] key=([\w.-]+) value=(-?[\d.]+)', read(LOG)):
        key, value = m.group(1), float(m.group(2))
        if key in section_keys:
            found[key] = value
        else:
            unknown.append(key)
    return found, sorted(set(unknown))


# --------------------------------------------------------------------------
# Pins and rendering
# --------------------------------------------------------------------------

BAND_SOURCE = os.path.join(ROOT, "Source", "RiorsEdge", "Tests", "BreakerPowerBandTests.cpp")


def parse_band_edges():
    """The authored band edges, read out of the test that asserts them.

    THE NUMBER HAS ONE HOME AND IT IS THE ASSERTION. Seven of these edges used
    to exist twice — once as a constexpr in the test that fails on them, once as
    a number in the pin file that reports on them — and the two could disagree
    silently. That is not hypothetical: the at-cap pin's own entry anticipates
    its band moving ("either the authored 8-10x moves, or content retunes until
    the measurement reaches it"), and moving it would have left the test
    asserting the old edges, so the report would have gone green while the suite
    stayed red on a target nobody held any more.

    The direction follows the lane register above, which lives in the C++ header
    and is parsed here for the reason stated at its own site: a list in this
    file would be a second place to update and the first one to rot. The script
    is a reporter, and a reporter must not be an authority.

    What does NOT move is the `why` prose, which stays in the pin file. The two
    are different artefacts. The pin file records the DECISION — who looked,
    when, and why a number was allowed to move — which is what the
    re-pinning-is-not-widening rule at the top of that file is about. The number
    itself is an assertion and belongs beside the code making it.
    """
    text = read(BAND_SOURCE)
    edges = {m.group(1): float(m.group(2)) for m in re.finditer(
        r"constexpr\s+float\s+(\w+)\s*=\s*(-?\d+(?:\.\d+)?)f\s*;", text)}
    if not edges:
        raise ParseError(f"{BAND_SOURCE}: no constexpr float band edges parsed.")
    return edges


def load_pins(section_directions):
    """Read the pin file, and refuse a pin that cannot do its job.

    Two ways a pin is inert rather than wrong-valued, and both are silent:

    A key that matches no section is never consulted, which looks exactly like
    a section somebody decided to leave as measurement-only. A typo could
    disarm a ratchet with no signal at all.

    A pin whose SHAPE does not match its section's DIRECTION is half-read: a
    ceiling section with only a `min` is never checked, and a band section with
    only a `max` silently stops guarding its lower edge.

    `kind` is deliberately NOT validated. It says WHY a pin sits where it does
    — measurement or target — and that is orthogonal to shape. A band section's
    measurement pin is legitimately two-sided; an earlier version of this
    function conflated the two and rejected exactly that, which is how it came
    to be written down here.
    """
    if not os.path.isfile(PINS):
        return {}
    pins = json.loads(read(PINS))
    unknown = [k for k in pins
               if not k.startswith("_") and k not in section_directions]
    if unknown:
        raise ParseError(
            "pin file names sections that do not exist: " + ", ".join(sorted(unknown))
            + ". A pin matching no section is silently inert — fix the key or "
              "delete the pin.")
    # Resolve every `source` edge before the shape check, so a pin whose symbol
    # has been renamed or deleted is REFUSED rather than silently losing an
    # edge. Same discipline as the unknown-key check above: a pin that cannot do
    # its job stops the report instead of quietly doing nothing.
    edges = None
    for key, pin in pins.items():
        if key.startswith("_") or "source" not in pin:
            continue
        if edges is None:
            edges = parse_band_edges()
        for bound, symbol in pin["source"].items():
            if symbol not in edges:
                raise ParseError(
                    f"pin '{key}' sources its {bound} from '{symbol}', which is not a "
                    f"constexpr float in {os.path.relpath(BAND_SOURCE, ROOT)}. The edge "
                    "would be unset and never checked — fix the symbol or inline the number.")
            pin[bound] = edges[symbol]

    for key, pin in pins.items():
        if key.startswith("_"):
            continue
        need = {CEILING: {"max"}, FLOOR: {"min"}, BAND: {"min", "max"}}[section_directions[key]]
        missing = need - set(pin)
        if missing:
            raise ParseError(
                f"pin '{key}' is a {section_directions[key]} and is missing "
                + ", ".join(sorted(missing))
                + ". The unset edge would never be checked.")
    return pins


def judge(section, pin):
    """Returns (state, message). State is one of ok / violated / unpinned.

    A pin may carry a `target` alongside its limit. The limit is what FAILS the
    build; the target is the direction of travel. A number that only has a
    ceiling at its current value cannot grow, but it also has no reason to
    shrink, and a section like that will sit where it is for a year.
    """
    if pin is None:
        return "unpinned", "no pin — measurement only"
    v, d = section["value"], section["direction"]
    tgt = f", target {pin['target']}" if "target" in pin else ""
    if d == CEILING:
        limit = pin["max"]
        return ("ok" if v <= limit else "violated"), f"ceiling {limit}{tgt}"
    if d == FLOOR:
        limit = pin["min"]
        return ("ok" if v >= limit else "violated"), f"floor {limit}{tgt}"
    lo, hi = pin["min"], pin["max"]
    return ("ok" if lo <= v <= hi else "violated"), f"band {lo}–{hi}{tgt}"


def render(sections, asserted, suite, pins, emitted, unknown_emitted):
    L = []
    a = L.append
    a("# State")
    a("")
    a("Generated by `make status`. Every number here is measured.")
    a("Do not edit this file; edit the generator or the thing it measures.")
    # No commit stamp, deliberately. A generated file that carries the hash it
    # was generated from is permanently dirty after every commit, and a file
    # that always shows a diff is one people stop reading diffs on — which
    # would delete the entire reason this one is committed. Git already knows
    # when it was generated and from what.
    a("")
    a("Sections declare a direction. **Ceiling** falls and never rises. **Floor** rises")
    a("and never falls. **Band** stays inside. An unpinned section reports its")
    a("measurement without judging it.")
    a("")

    violations = []

    a("## Summary")
    a("")
    a("| Section | Direction | Value | Pin | State |")
    a("|---|---|---|---|---|")
    for s in sections:
        state, pintext = judge(s, pins.get(s["key"]))
        if state == "violated":
            violations.append(s["key"])
        mark = {"ok": "ok", "violated": "**OUT**", "unpinned": "—"}[state]
        a(f"| {s['title']} | {s['direction']} | {s['value']} {s['unit']} | {pintext} | {mark} |")
    for key, title, direction, test in EMITTED_BY_TEST:
        if key in emitted:
            fake = {"value": round(emitted[key], 2), "direction": direction}
            state, pintext = judge(fake, pins.get(key))
            if state == "violated":
                violations.append(key)
            mark = {"ok": "ok", "violated": "**OUT**", "unpinned": "—"}[state]
            a(f"| {title} | {direction} | {fake['value']} | {pintext} | {mark} |")
        else:
            a(f"| {title} | {direction} | not emitted | — | needs `{test}` to emit it |")
    a("")

    if unknown_emitted:
        a("**Status lines nobody reads:** " + ", ".join(f"`{k}`" for k in unknown_emitted))
        a("")
        a("The suite emitted these keys and no section claims them. Either add the")
        a("section or stop emitting — an unread status line is a silent nothing.")
        a("")

    if suite is not None:
        a("## Tests")
        a("")
        a(f"- passing: {len(suite['passed'])}")
        a(f"- expected red: {len(suite['expected_red'])}")
        a(f"- **unexpected red: {len(suite['unexpected_red'])}**")
        a(f"- asserted invariants with no test: "
          f"{next(s['value'] for s in sections if s['key'] == 'unimplemented-invariants')}")
        a("")
        a("Expected-red, unexpected-red and unimplemented are three different states.")
        a("A test that was never written is the worst of the three: it looks asserted.")
        a("")
        for label, items in (("Unexpected red", sorted(suite["unexpected_red"])),
                             ("Expected red", sorted(suite["expected_red"])),
                             ("No longer red — delete its entry", sorted(suite["no_longer_red"]))):
            if items:
                a(f"**{label}**")
                a("")
                for i in items:
                    a(f"- `{i}`")
                a("")
        if suite["unexpected_red"]:
            violations.append("unexpected-red")
    else:
        a("## Tests")
        a("")
        a("No suite log found. Run the suite, then regenerate.")
        a("")

    for s in sections:
        state, pintext = judge(s, pins.get(s["key"]))
        a(f"## {s['title']}")
        a("")
        a(f"**{s['direction']}** · {s['value']} {s['unit']} · {pintext}"
          + ("  ·  **OUT**" if state == "violated" else ""))
        a("")
        a(s["note"])
        a("")
        if s["detail"]:
            shown = s["detail"][:40]
            for d in shown:
                a(f"- {d}")
            if len(s["detail"]) > len(shown):
                a(f"- …and {len(s['detail']) - len(shown)} more")
            a("")

    return "\n".join(L) + "\n", violations


def main():
    try:
        sources = load_sources()
        sections, asserted = build_sections(sources)
    except ParseError as e:
        sys.stderr.write(f"status: PARSE FAILURE — {e}\n")
        sys.stderr.write("status: refusing to emit a report that would understate the "
                         "problem by reporting zero.\n")
        return 2

    try:
        directions = {sec["key"]: sec["direction"] for sec in sections}
        directions.update({k: d for k, _, d, _ in EMITTED_BY_TEST})
        directions["unexpected-red"] = CEILING
        pins = load_pins(directions)
    except ParseError as e:
        sys.stderr.write("status: PIN FAILURE - " + str(e) + os.linesep)
        return 2
    expected_red = set(pins.get("_expected_red", []))
    try:
        suite = parse_suite_log(expected_red, parse_declared_tests(sources))
    except ParseError as e:
        sys.stderr.write("status: SUITE LOG FAILURE - " + str(e) + os.linesep)
        sys.stderr.write("status: refusing to emit a report whose test totals would be "
                         "short by an unknown amount." + os.linesep)
        return 2
    emitted, unknown_emitted = parse_emitted({k for k, _, _, _ in EMITTED_BY_TEST})
    text, violations = render(sections, asserted, suite, pins, emitted, unknown_emitted)

    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)

    print(f"status: wrote {os.path.relpath(OUT, ROOT)}")
    if not pins:
        print("status: NO PINS. Every section is measurement-only until pins are "
              "authored in Scripts/status-pins.json.")
        print("status: do not auto-generate them from this run — where the current "
              "state is the problem, the pin is the target, not the measurement.")
        return 0
    if violations:
        print("status: OUT OF BAND — " + ", ".join(violations))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
