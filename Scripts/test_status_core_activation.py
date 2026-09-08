"""Focused reporter activation checks; synthetic Core, actual current Doctrine rows.
Run with python -B Scripts/test_status_core_activation.py. No engine or report writes.
"""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

MODULE = Path(__file__).with_name("status.py")
spec = importlib.util.spec_from_file_location("status_core_activation", MODULE)
status = importlib.util.module_from_spec(spec)
spec.loader.exec_module(status)
# In a staged checkout, pass RIORS_EDGE_REPO to use the live Doctrine fixture.
import os
REPO = Path(os.environ.get("RIORS_EDGE_REPO", str(MODULE.parent.parent)))


class CoreActivationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.saved = status.ROOT, status.SRC, status.DATA, status.SPEC
        self.addCleanup(self.restore_globals)
        root = Path(self.temp.name)
        status.ROOT = str(root)
        status.SRC = str(root / "Source/RiorsEdge")
        status.SPEC = str(root / "Docs/spec")
        status.DATA = str(root / "Data/progression.json")
        self.roster_spec = (REPO / "Docs/spec/core-wheel.md").read_text(encoding="utf-8")
        self.original_lib = (REPO / "Source/RiorsEdge/Progression/BreakerProgressionLibrary.cpp").read_text(encoding="utf-8")
        self.lib = self.original_lib.replace("UBreakerProgressionLibrary::GetCoreSliceTree()", "UBreakerProgressionLibrary::GetLegacyCoreTree()")
        self.lib = 'UBreakerProgressionTree* UBreakerProgressionLibrary::GetCoreSliceTree()\n{\n return BreakerCoreRoster::BuildCandidate(GetTransientPackage(), Error);\n}\n' + self.lib
        self.census = json.loads((REPO / "Data/progression.json").read_text(encoding="utf-8"))
        self.census["trees"] = [tree for tree in self.census["trees"] if tree["currency"] == "DoctrinePoints"]
        wedges = status.re.findall(r'^\*\*([A-Za-z]+) \((major|minor)\)\*\*:', self.roster_spec, status.re.M)
        nodes = []
        authoring = []
        for wedge, kind in wedges:
            ranks = [1, 3, 1, 3, 1, 3, 1, 1, 1, 1, 1] if kind == "major" else [1, 3, 1, 3, 1, 1]
            costs = [1, 1, 2, 1, 2, 1, 2, 1, 1, 3, 5] if kind == "major" else [1, 1, 2, 1, 2, 2]
            for i, (rank, cost) in enumerate(zip(ranks, costs)):
                node_id = f"Core.{wedge}.Fixture{i}"
                nodes.append(dict(id=node_id, tier=1, ranks=rank, cost=cost, effects=[], tags=[], cornerstone=False, constellation=wedge))
                authoring.append(f'Node(Outer, TEXT("{node_id}"), TEXT("Name"), TEXT("Description"));')
        self.core = dict(id="Core.Slice", currency="CorePoints", nodes=nodes, coreWedgeOrder=[w for w, _ in wedges])
        self.census["trees"].insert(0, self.core)
        self.sources = {}
        for rel in status.CORE_SOURCE_FIXED + ("Progression/BreakerCoreRoster.cpp", "Progression/BreakerCoreRoster.h", "Progression/BreakerCoreRosterFixture.cpp"):
            path = Path(status.SRC) / rel
            text = self.lib if rel == status.LIB else "// fixture source\n"
            if rel.endswith("RosterFixture.cpp"):
                text = "\n".join(authoring)
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text, encoding="utf-8")
            self.sources[str(path)] = text
        # Tag aliases must resolve exactly as the real independent Doctrine parser.
        real_sources = {str(p): p.read_text(encoding="utf-8", errors="surrogateescape")
                        for p in (REPO / "Source/RiorsEdge").rglob("*") if p.suffix in (".cpp", ".h")}
        previous_src = status.SRC
        status.SRC = str(REPO / "Source/RiorsEdge")
        self.tags = status.parse_tag_map(real_sources)
        status.SRC = previous_src
        self.census["coreAuthoringSources"] = status.expected_core_source_hashes(self.sources)

    def restore_globals(self):
        status.ROOT, status.SRC, status.DATA, status.SPEC = self.saved

    def inventory(self):
        return status.activated_core_inventory(self.sources, self.census, self.tags, self.roster_spec)

    def test_live_detection_ignores_comments_strings_and_unregistered_candidate(self):
        legacy = "UBreakerProgressionTree* UBreakerProgressionLibrary::GetCoreSliceTree() { return Tree; } void Unregistered() { BreakerCoreRoster::BuildCandidate(); }"
        self.assertFalse(status.core_builder_is_live(legacy))
        self.assertFalse(status.core_builder_is_live('UBreakerProgressionTree* UBreakerProgressionLibrary::GetCoreSliceTree() { /* BreakerCoreRoster::BuildCandidate(); */ const char* x="BreakerCoreRoster::BuildCandidate()"; return Tree; }'))
        self.assertTrue(status.core_builder_is_live(self.lib))
        self.assertIsNone(status.activated_core_inventory({str(Path(status.SRC) / status.LIB): legacy}, None, {}, self.roster_spec))

    def test_native_core_replaces_all_dead_legacy_literals(self):
        nodes, census = self.inventory()
        self.assertEqual(len([n for n in nodes if n["id"].startswith("Core.")]), 187)
        self.assertEqual(len(nodes), 187 + sum(len(t["nodes"]) for t in self.census["trees"] if t["currency"] == "DoctrinePoints"))
        self.assertNotIn("Core.Precision.Sightline", {n["id"] for n in nodes})
        self.assertTrue(all(n["tree"] == "Core.Slice" for n in nodes if n["id"].startswith("Core.")))

    def test_missing_or_stale_witness_fails(self):
        self.census.pop("coreAuthoringSources")
        with self.assertRaisesRegex(status.ParseError, "witness"):
            self.inventory()
        self.census["coreAuthoringSources"] = status.expected_core_source_hashes(self.sources)
        path = Path(status.SRC) / "Progression/BreakerCoreRosterFixture.cpp"
        path.write_text(path.read_text() + "\n// magnitude changed", encoding="utf-8")
        with self.assertRaisesRegex(status.ParseError, "witness"):
            self.inventory()

    def test_missing_sector_and_wrong_node_identity_fail(self):
        old = self.core["nodes"].pop()
        with self.assertRaisesRegex(status.ParseError, "identities"):
            self.inventory()
        self.core["nodes"].append(old)
        self.core["nodes"][0]["id"] = "Core.Wrong.Identity"
        with self.assertRaisesRegex(status.ParseError, "identities"):
            self.inventory()

    def test_wrong_roster_order_and_stale_cost_fail(self):
        self.core["coreWedgeOrder"].reverse()
        with self.assertRaisesRegex(status.ParseError, "wedge order"):
            self.inventory()
        self.core["coreWedgeOrder"].reverse()
        self.core["nodes"][0]["cost"] += 1
        with self.assertRaisesRegex(status.ParseError, "offers"):
            self.inventory()

    def test_doctrine_payload_and_identity_drift_fail(self):
        doctrine = self.census["trees"][1]["nodes"]
        doctrine[0]["cost"] += 1
        with self.assertRaisesRegex(status.ParseError, "Doctrine source/native census mismatch"):
            self.inventory()
        doctrine[0]["cost"] -= 1
        doctrine.pop()
        with self.assertRaisesRegex(status.ParseError, "Doctrine source/native census identities"):
            self.inventory()

    def test_automatic_load_uses_census_and_mixed_getter_is_refused(self):
        path = Path(status.DATA); path.parent.mkdir(parents=True)
        path.write_text(json.dumps(self.census), encoding="utf-8")
        nodes, _ = status.activated_core_inventory(self.sources, None, self.tags, self.roster_spec)
        self.assertEqual(sum(n["id"].startswith("Core.") for n in nodes), 187)
        with self.assertRaisesRegex(status.ParseError, "mixes"):
            status.core_builder_is_live('UBreakerProgressionTree* UBreakerProgressionLibrary::GetCoreSliceTree() { MakeNode(); return BreakerCoreRoster::BuildCandidate(); }')


if __name__ == "__main__":
    unittest.main()
