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
LOG = os.path.join(ROOT, "Saved", "Logs", "riors_edge.log")

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
    return targets, paid


def parse_conditions(text):
    m = re.search(r'enum class EBreakerBuildCondition\s*:\s*uint8\s*\{(.*?)\}\s*;', text, re.S)
    if not m:
        raise ParseError("EBreakerBuildCondition enum not found.")
    out = [c.strip() for c in re.findall(r'^\s*(\w+)', m.group(1), re.M)]
    return [c for c in out if c not in ("Count",)]


def parse_declared_tags(lib_text):
    return re.findall(r'UE_DEFINE_GAMEPLAY_TAG\((\w+),', lib_text)


# --------------------------------------------------------------------------
# Consumption — BOTH axes
# --------------------------------------------------------------------------
# A node is live if its tag has a consumer OR its node id does. The project
# reads both: HasNodeTag(BreakerNodeTags::Node_X) and
# GetClassNodeRank(BreakerSteadyNodeId). A report that greps only tags calls
# live nodes dead, which is how a previous audit reached "86 of 94 tags have no
# consumer" and overstated the problem.

def build_consumer_index(sources):
    prod = non_test_sources(sources, exclude_substrings=("Progression/BreakerProgressionLibrary",))
    return "\n".join(prod.values())


def tag_consumed(index, tag):
    return re.search(r'\b' + re.escape(tag) + r'\b', index) is not None


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


def parse_declared_tests(sources):
    names = set()
    for path, text in sources.items():
        if "Tests" not in path:
            continue
        for m in re.finditer(
                r'IMPLEMENT_\w*AUTOMATION_TEST\(\s*\w+\s*,\s*"([^"]+)"', text, re.S):
            names.add(m.group(1))
    if not names:
        raise ParseError("No automation tests found. The IMPLEMENT_ macro shape changed.")
    return names


def normalise_test(name):
    return name[len("RiorsEdge."):] if name.startswith("RiorsEdge.") else name


# --------------------------------------------------------------------------
# Suite log — expected red, unexpected red
# --------------------------------------------------------------------------

def parse_suite_log(expected_red):
    if not os.path.isfile(LOG):
        return None
    text = read(LOG)
    passed = set(re.findall(r'Result=\{Success\} Name=\{[^}]*\} Path=\{([^}]+)\}', text))
    failed = set(re.findall(r'Result=\{Fail\} Name=\{[^}]*\} Path=\{([^}]+)\}', text))
    exp = {t for t in failed if normalise_test(t) in expected_red}
    unexp = failed - exp
    missing_red = {t for t in expected_red if "RiorsEdge." + t in passed}
    return {"passed": passed, "expected_red": exp,
            "unexpected_red": unexp, "no_longer_red": missing_red}


# --------------------------------------------------------------------------
# Sections
# --------------------------------------------------------------------------

def classify(node):
    if node["cornerstone"] or node["cost"] >= 3:
        return "convergence/keystone"
    if node["ranks"] > 1:
        return "ranked minor"
    return "notable"


def build_sections(sources):
    lib = sources[os.path.join(SRC, *LIB.split("/"))]
    types = sources[os.path.join(SRC, *TYPES.split("/"))]
    conds_text = read(os.path.join(SRC, "Progression", "BreakerBuildConditions.h"))

    nodes = parse_nodes(lib)
    targets, paid = parse_lane_register(types)
    conditions = parse_conditions(conds_text)
    declared_tags = parse_declared_tags(lib)
    index = build_consumer_index(sources)

    sections = []

    # --- silent nodes -----------------------------------------------------
    silent = []
    for n in nodes:
        pays = any(e in paid for e in n["effects"])
        heard = any(tag_consumed(index, t) for t in n["tags"]) or id_consumed(index, n["id"])
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
    unmapped = [t for t in targets if t not in paid]
    authored_targets = {e for n in nodes for e in n["effects"]}
    empty_lanes = [t for t in paid if t not in authored_targets]
    sections.append({
        "key": "unmapped-stat-targets", "title": "Stat targets with no aggregation lane",
        "direction": CEILING, "value": len(unmapped), "unit": f"of {len(targets)}",
        "detail": unmapped,
        "note": "A node authored against one of these is silently unpaid.",
    })
    sections.append({
        "key": "empty-lanes", "title": "Aggregation lanes carrying nothing",
        "direction": CEILING, "value": len(empty_lanes), "unit": f"of {len(paid)} lanes",
        "detail": empty_lanes,
        "note": "Plumbing with no author. Not harmful, but not free either.",
    })

    # --- dead tags --------------------------------------------------------
    dead_tags = [t for t in declared_tags if not tag_consumed(index, t)]
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
    notifies = []
    for rel, text in non_test_sources(sources).items():
        if not rel.startswith("Classes/") or not rel.endswith(".h"):
            continue
        for m in re.finditer(r'\bvoid (Notify\w+)\s*\(', text):
            notifies.append((rel, m.group(1)))
    uncalled = []
    for rel, fn in notifies:
        callers = [r for r, t in non_test_sources(sources).items()
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
    sections.append({
        "key": "node-shape-composition", "title": "Node-shape composition, per tree",
        "direction": BAND, "value": round(100 * all_minor / max(1, len(nodes))),
        "unit": "% ranked minors, all trees",
        "detail": comp_rows,
        "note": "A tree that is almost entirely notable-shaped has nothing to fill a "
                "constellation with between the interesting picks.",
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
    ("loot-per-hour", "Loot per hour, by area level", BAND,
     "RiorsEdge.Items.Drops.LootPerHour"),
    ("power-band-atcap", "Build variance band, at cap", BAND,
     "RiorsEdge.Progression.PowerBand.AtCap"),
    ("power-band-endgame", "Build variance band, endgame", BAND,
     "RiorsEdge.Progression.PowerBand.Endgame"),
    ("power-band-ability", "Build variance band, ability lane", BAND,
     "RiorsEdge.Progression.PowerBand.AbilityLane"),
    ("rewrite-impact", "Rewrite impact, per band", BAND,
     "RiorsEdge.Progression.RuleBandImpact"),
]


# --------------------------------------------------------------------------
# Pins and rendering
# --------------------------------------------------------------------------

def load_pins(section_keys):
    """Read the pin file, and refuse a pin that names no section.

    A pin key that matches nothing is silently unpinned, which looks exactly
    like a section somebody decided to leave as measurement-only. That is the
    same shape as every other silent-nothing this project has shipped, so a
    typo here fails loudly rather than quietly disarming a ratchet.

    `kind` is documentation for a human reading the file. The generator branches
    on the section's DIRECTION, declared in build_sections, and never on `kind` —
    so a two-sided pin takes the band path because its section says band, not
    because its pin says so. The two must not be allowed to disagree.
    """
    if not os.path.isfile(PINS):
        return {}
    pins = json.loads(read(PINS))
    unknown = [k for k in pins
               if not k.startswith("_") and k not in section_keys]
    if unknown:
        raise ParseError(
            "pin file names sections that do not exist: " + ", ".join(sorted(unknown))
            + ". A pin matching no section is silently inert — fix the key or "
              "delete the pin.")
    for key, pin in pins.items():
        if key.startswith("_"):
            continue
        kind = pin.get("kind")
        two_sided = "min" in pin and "max" in pin
        if kind == "measurement" and two_sided:
            raise ParseError(
                f"pin '{key}' is documented as a measurement but is two-sided. "
                "A measurement pin holds one edge; say which.")
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


def git_head():
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"], cwd=ROOT).decode().strip()
    except Exception:
        return "unknown"


def render(sections, asserted, suite, pins):
    L = []
    a = L.append
    a("# State")
    a("")
    a(f"Generated by `make status` from `{git_head()}`. Every number here is measured.")
    a("Do not edit this file; edit the generator or the thing it measures.")
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
        a(f"| {title} | {direction} | not emitted | — | needs `{test}` to log it |")
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
        pins = load_pins({sec["key"] for sec in sections}
                         | {k for k, _, _, _ in EMITTED_BY_TEST}
                         | {"unexpected-red"})
    except ParseError as e:
        sys.stderr.write("status: PIN FAILURE - " + str(e) + os.linesep)
        return 2
    expected_red = set(pins.get("_expected_red", []))
    suite = parse_suite_log(expected_red)
    text, violations = render(sections, asserted, suite, pins)

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
