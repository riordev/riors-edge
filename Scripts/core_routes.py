"""Measure the activated Core's purchase topology from its native census.

Reads actual roles, ranks, costs, prerequisites, groups, local gates, adjacency
and wedge order. Measures affordable routing, not effect strength or good builds.
No engine, generated-document or pin writes. Usage: python -B Scripts/core_routes.py
[--census Data/progression.json]. Legacy or unsupported routing fails explicitly.
"""
import argparse
from collections import Counter, deque
import hashlib
import json
from pathlib import Path
import sys


class RouteError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise RouteError(message)


def integer(value, label, minimum=0):
    require(isinstance(value, int) and not isinstance(value, bool) and value >= minimum,
            f"{label}: expected integer >= {minimum}, got {value!r}")
    return value


def check_freshness(census, repo):
    witness = census.get("coreAuthoringSources")
    require(isinstance(witness, dict) and witness, "Native Core source witness missing. Rebuild/export the activated census first.")
    for relative, digest in witness.items():
        path = (repo / relative).resolve()
        require(path.is_relative_to(repo.resolve()), f"Source witness escapes repository: {relative}")
        require(path.is_file(), f"Source witness file missing: {relative}")
        actual = hashlib.sha1(path.read_bytes()).hexdigest()
        require(isinstance(digest, str) and digest.lower() == actual, f"Stale native census source witness: {relative}; rebuild/export first.")


def read_core(census):
    budget = integer(census.get("budgets", {}).get("core"), "Core budget", 1)
    trees = [t for t in census.get("trees", []) if t.get("currency") == "CorePoints"]
    require(len(trees) == 1 and trees[0].get("id") == "Core.Slice", "Expected one activated live Core.Slice tree.")
    tree = trees[0]
    require(tree.get("restrictEntryToOwnedNeighbor") is True, "Legacy/unrestricted Core input: neighbour-entry policy is required.")
    order = tree.get("coreWedgeOrder", [])
    require(len(order) >= 3 and len(set(order)) == len(order), "Replacement Core requires a distinct cyclic wedge order.")
    nodes = tree.get("nodes", [])
    by_id = {n.get("id"): n for n in nodes}
    require(len(by_id) == len(nodes) and all(isinstance(i, str) and i for i in by_id), "Missing or duplicate native node IDs.")
    sectors = {s["name"]: s["sector"] for s in tree.get("constellations", [])}
    require(set(sectors) == set(order) and all(s and s != "None" for s in sectors.values()), "Native sector metadata does not cover the wedge roster.")
    require({n.get("constellation") for n in nodes} == set(order), "Native nodes and wedge roster disagree.")
    roles = {"Gateway", "LaneMinor", "LaneNotable", "Link", "Convergence", "Keystone"}
    adjacency = {i: set() for i in by_id}
    edge_set = set()
    for edge in tree.get("adjacencyEdges", []):
        a, b = edge["a"], edge["b"]
        require(a in by_id and b in by_id and a != b, f"Invalid adjacency edge {a} -> {b}")
        pair = frozenset((a, b))
        require(pair not in edge_set, f"Duplicate adjacency edge {a} -> {b}")
        edge_set.add(pair); adjacency[a].add(b); adjacency[b].add(a)
    entries = tree.get("entryNodes", [])
    require(len(entries) == len(set(entries)), "Duplicate gateway entries.")
    gateways = {}
    for n in nodes:
        name = n["id"]
        require(n.get("coreRole") in roles, f"{name}: missing replacement role (legacy census is not measurable).")
        integer(n.get("ranks"), f"{name} ranks", 1); integer(n.get("cost"), f"{name} cost", 1)
        integer(n.get("coreLaneIndex"), f"{name} lane index")
        require(not n.get("cornerstone") and not n.get("requiredTreeInvestment", 0),
                f"{name}: global/commitment gate prevents independent local decomposition.")
        require(not n.get("exclusive", []), f"{name}: exclusive allocations need a different sector completeness definition.")
        integer(n.get("requiredConstellationInvestment", 0), f"{name} local gate")
        if n["coreRole"] == "Gateway":
            require(n["constellation"] not in gateways, f"Multiple gateways in {n['constellation']}")
            require(n["ranks"] == 1 and not n.get("prerequisites", []) and not n.get("prerequisiteGroups", [])
                    and not n.get("requiredConstellationInvestment", 0), f"{name}: gateway is not freely purchasable at entry.")
            gateways[n["constellation"]] = name
        all_requirements = list(n.get("prerequisites", []))
        for group in n.get("prerequisiteGroups", []):
            candidates = group.get("candidates", [])
            count = integer(group.get("minimumSatisfied"), f"{name} group minimum", 1)
            require(count <= len(candidates) and len({p['id'] for p in candidates}) == len(candidates), f"{name}: invalid prerequisite group.")
            all_requirements.extend(candidates)
        for prerequisite in all_requirements:
            dep = by_id.get(prerequisite.get("id"))
            require(dep is not None and dep["constellation"] == n["constellation"], f"{name}: foreign/missing prerequisite prevents local decomposition.")
            rank = integer(prerequisite.get("rank"), f"{name} prerequisite rank", 1)
            require(rank <= dep["ranks"], f"{name}: prerequisite rank exceeds {dep['id']} capacity.")
    require(set(gateways) == set(order) and set(entries) == set(gateways.values()), "Entry nodes must be exactly one gateway per native wedge.")
    expected_ring = {frozenset((gateways[w], gateways[order[(i + 1) % len(order)]])) for i, w in enumerate(order)}
    actual_ring = {edge for edge in edge_set if len({by_id[i]['constellation'] for i in edge}) > 1}
    require(actual_ring == expected_ring, "Cross-wedge edges are not exactly the native cyclic gateway ring; contiguous-arc DP would be invalid.")
    wedges = []
    for name in order:
        rows = [n for n in nodes if n["constellation"] == name]
        counts = Counter(n["coreRole"] for n in rows)
        require(counts["Gateway"] == counts["Convergence"] == 1 and counts["Keystone"] <= 1,
                f"{name}: unsupported gateway/convergence/keystone role multiplicity.")
        minors = {n["coreLaneIndex"] for n in rows if n["coreRole"] == "LaneMinor"}
        notables = {n["coreLaneIndex"] for n in rows if n["coreRole"] == "LaneNotable"}
        require(minors and minors == notables and len(minors) == counts["LaneMinor"] == counts["LaneNotable"],
                f"{name}: lane minor/notable metadata does not pair uniquely.")
        wedges.append(dict(name=name, sector=sectors[name], nodes=rows, adjacency=adjacency))
    return budget, wedges


def local_states(wedge):
    """Actual reachable purchase states; every transition buys one real rank."""
    rows = wedge["nodes"]; index = {n["id"]: i for i, n in enumerate(rows)}
    gateway = next(i for i, n in enumerate(rows) if n["coreRole"] == "Gateway")
    neighbours = [{index[i] for i in wedge["adjacency"][n['id']] if i in index} for n in rows]
    size = 1
    for n in rows: size *= n["ranks"] + 1
    require(size <= 500000, f"{wedge['name']}: local state space {size} exceeds bounded exhaustive measurement.")
    empty = tuple(0 for _ in rows)
    queue = deque([empty]); seen = {empty}; result = []
    while queue:
        ranks = queue.popleft()
        cost = sum(r * n["cost"] for r, n in zip(ranks, rows))
        if ranks[gateway]:
            result.append(dict(cost=cost, key=sum(r > 0 for r, n in zip(ranks, rows) if n['coreRole'] == 'Keystone'),
                               lane=any(r > 0 and n['coreRole'] == 'LaneNotable' for r, n in zip(ranks, rows)),
                               convergence=any(r > 0 and n['coreRole'] == 'Convergence' for r, n in zip(ranks, rows)),
                               ranks=ranks))
        for i, n in enumerate(rows):
            if ranks[i] >= n['ranks'] or cost < n.get('requiredConstellationInvestment', 0): continue
            if i == gateway:
                if ranks[i]: continue
            elif not any(ranks[j] for j in neighbours[i]): continue
            if any(ranks[index[p['id']]] < p['rank'] for p in n.get('prerequisites', [])): continue
            if any(sum(ranks[index[p['id']]] >= p['rank'] for p in g['candidates']) < g['minimumSatisfied'] for g in n.get('prerequisiteGroups', [])): continue
            updated = list(ranks); updated[i] += 1; updated = tuple(updated)
            if updated not in seen: seen.add(updated); queue.append(updated)
    full = tuple(n['ranks'] for n in rows)
    require(full in seen, f"{wedge['name']}: full authored ranks are unreachable under native gates/adjacency.")
    return result


def minimum(states, predicate, label):
    eligible = [s['cost'] for s in states if predicate(s)]
    require(eligible, f"No legal allocation reaches {label}")
    return min(eligible)


def frontier(wedges, locals_, budget, depth, keys):
    options = []
    for local in locals_:
        best = {}
        for state in local:
            signature = state['key'], 1 if depth == 'gateway' else int(state[depth])
            if signature not in best or state['cost'] < best[signature]['cost']: best[signature] = state
        options.append(best)
    result = None
    for start in range(len(wedges)):
        states = {(0, 0): (0, [])}
        for offset in range(len(wedges)):
            i = (start + offset) % len(wedges); following = {}
            for (oldk, oldq), (cost, witness) in states.items():
                for (addk, addq), local in options[i].items():
                    k, q, price = oldk + addk, oldq + addq, cost + local['cost']
                    if k > keys or price > budget: continue
                    if (k, q) not in following or price < following[k, q][0]:
                        allocation = {n['id']: rank for n, rank in zip(wedges[i]['nodes'], local['ranks']) if rank}
                        following[k, q] = price, witness + [dict(wedge=wedges[i]['name'], cost=local['cost'], ranks=allocation)]
            states = following
            for (k, q), (cost, witness) in states.items():
                if k == keys and (result is None or (q, -cost) > (result['wedges'], -result['cost'])):
                    result = dict(depth=depth, keystones=keys, wedges=q, cost=cost, witness=witness)
    return result


def measure(census):
    budget, wedges = read_core(census)
    locals_ = [local_states(w) for w in wedges]
    summaries = []
    for wedge, states in zip(wedges, locals_):
        rows = wedge['nodes']
        major = any(n['coreRole'] == 'Keystone' for n in rows)
        summaries.append(dict(wedge=wedge['name'], sector=wedge['sector'], nodes=len(rows), major=major,
            gateway=minimum(states, lambda s: True, 'gateway'),
            one_lane=minimum(states, lambda s: s['lane'], 'one completed lane'),
            convergence=minimum(states, lambda s: s['convergence'], 'convergence'),
            all_nonkeystone_nodes_rank1=minimum(states, lambda s: all(r >= 1 for r,n in zip(s['ranks'],rows) if n['coreRole'] != 'Keystone') and not s['key'], 'all non-keystone nodes'),
            all_nodes_minimum_including_keys=minimum(states, lambda s: all(r >= 1 for r in s['ranks']), 'all nodes'),
            keystone=minimum(states, lambda s: s['key'], 'keystone') if major else None,
            full_ranks=sum(n['ranks']*n['cost'] for n in rows)))
    sectors = []
    for sector in dict.fromkeys(w['sector'] for w in wedges):
        rows = [s for s in summaries if s['sector'] == sector]
        sectors.append(dict(sector=sector, nodes=sum(s['nodes'] for s in rows), gateways=sum(s['gateway'] for s in rows),
            one_lane=sum(s['one_lane'] for s in rows), all_convergences=sum(s['convergence'] for s in rows),
            all_nonkeystone_nodes_rank1=sum(s['all_nonkeystone_nodes_rank1'] for s in rows),
            all_nodes_minimum_including_keys=sum(s['all_nodes_minimum_including_keys'] for s in rows),
            full_ranks=sum(s['full_ranks'] for s in rows)))
    offered = sum(s['full_ranks'] for s in summaries)
    key_prices = sorted(s['keystone'] for s in summaries if s['keystone'] is not None)
    return dict(budget=budget, nodes=sum(s['nodes'] for s in summaries), offered=offered, density=offered/budget,
        scope='Native purchase topology only. Sector sums omit outside transit; cyclic frontiers include transit. No effect-strength or build-quality claim.',
        sectors=sectors, wedges=summaries,
        three_keystone_local_lower_bound=sum(key_prices[:3]) if len(key_prices) >= 3 else None,
        three_keystones_exceed_budget=len(key_prices) >= 3 and sum(key_prices[:3]) > budget,
        frontiers=[frontier(wedges, locals_, budget, depth, keys) for depth in ('gateway','lane','convergence') for keys in (0,1,2)])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--census', type=Path, default=Path(__file__).resolve().parent.parent/'Data/progression.json')
    args = parser.parse_args()
    try:
        census = json.loads(args.census.read_text(encoding='utf-8-sig'))
        # Fail the role/routing contract before checking freshness: legacy input
        # gets an actionable explanation rather than a cryptic missing key.
        read_core(census)
        check_freshness(census, args.census.resolve().parent.parent)
        report = measure(census)
        print(json.dumps(report, indent=2))
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f'core-routes: REFUSED: {error}', file=sys.stderr)
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main())
