"""Imports the Fernhall approach yard and creates its map.

The yard is authored in Scripts/compose_fernhall.py, which bakes every
instance's world transform into its vertices and names each mesh under the
prefix contract (blk_full_ / blk_chest_ / wall_ / flr_ / dress_ / marker_).
This script is the other half of that contract: it splits the GLB into
per-name static meshes under /Game/Breaker/Meshes/fernhall_yard, gives the
solid prefixes complex-as-simple collision (the kit meshes ship no simple
collision, and a wall that renders but does not block is worse than no wall),
and prints each mesh's imported bounds so the bounds-recovery claim is
CHECKED here rather than assumed — UBreakerZoneBuilder spawns everything at
identity, so if these bounds are not at the authored world positions the
whole route is broken and this output is where that shows first.

Run:
  UnrealEditor-Cmd.exe <project> -run=pythonscript -script="breaker_import_fernhall.py"
"""

import os
import unreal

PROJECT_DIR = unreal.SystemLibrary.get_project_directory()
GLB = os.path.normpath(os.path.join(PROJECT_DIR, "Assets", "zones", "fernhall_yard.glb"))
DEST = "/Game/Breaker/Meshes/fernhall_yard"
MAP_PACKAGE = "/Game/Breaker/Maps/Lvl_Fernhall"

EXPECTED_TOTAL = 58
SOLID_PREFIXES = ("blk_full_", "blk_chest_", "wall_", "flr_")

# THE MARKER CONTRACT, PARSED — not a fixed list of three names. This used to
# be MARKERS = ("marker_playerstart", "marker_rift", "marker_npc_contract"),
# one of three places that hardcoded "a zone has exactly one of each"; a yard
# with its own door could not be imported. Kept in step with
# BreakerZoneBuilder::ParseMarkerName, which is the C++ half of this contract.
#
#   marker_<role>          the entry yard
#   marker_<role>_<yard>   that yard
#
# LONGEST ROLE FIRST, for the same reason the C++ does it: npc_contract
# contains an underscore, so a shortest-match parse reads marker_npc_contract
# as role "npc" in a yard called "contract".
MARKER_ROLES = ("npc_contract", "playerstart", "rift")


def parse_marker(name):
    """(role, yard) for a marker name, or None if it is not one. yard is '' for
    the entry yard."""
    if not name.startswith("marker_"):
        return None
    rest = name[len("marker_"):]
    best = None
    for role in MARKER_ROLES:
        if not rest.startswith(role):
            continue
        # The role must end at a boundary, or "rift" would match "riftpad".
        if len(rest) != len(role) and rest[len(role)] != "_":
            continue
        if best is None or len(role) > len(best):
            best = role
    if best is None:
        return None
    return (best, rest[len(best) + 1:] if len(rest) > len(best) else "")


if not os.path.isfile(GLB):
    raise RuntimeError("[Fernhall] source GLB missing: %s (run Scripts/compose_fernhall.py)" % GLB)

# ---- The map. Idempotent, same as breaker_make_levels: never overwrite. ----
if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PACKAGE):
    level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level_sub.new_level(MAP_PACKAGE):
        raise RuntimeError("[Fernhall] could not create %s" % MAP_PACKAGE)
    unreal.log("[Fernhall] created %s" % MAP_PACKAGE)
else:
    unreal.log("[Fernhall] map exists, untouched: %s" % MAP_PACKAGE)

# ---- The meshes. Reimport in place on re-run. ------------------------------
task = unreal.AssetImportTask()
task.filename = GLB
task.destination_path = DEST
task.automated = True
task.replace_existing = True
task.save = False  # saved below, after collision settings are applied
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

registry = unreal.AssetRegistryHelpers.get_asset_registry()
assets = registry.get_assets_by_path(DEST, recursive=True)
meshes = {}
for data in assets:
    if data.asset_class_path.asset_name != "StaticMesh":
        continue
    meshes[str(data.asset_name)] = data

# THE ONE HARD REQUIREMENT IS A PLAYER START, and exactly one — the same rule
# FBreakerZoneMarkers::IsComplete enforces at load, checked here so a broken
# export fails at import rather than at spawn. Rift doors and mission givers
# are per-yard and optional; a marker-prefixed name that parses to no known
# role is a TYPO and refused, because the prefix is the contract.
parsed = {}
bad = []
for name in meshes:
    if not name.startswith("marker_"):
        continue
    role_yard = parse_marker(name)
    if role_yard is None:
        bad.append(name)
        continue
    parsed.setdefault(role_yard, []).append(name)

if bad:
    raise RuntimeError("[Fernhall] marker_ names that parse to no known role: %s "
                       "(roles: %s)" % (sorted(bad), MARKER_ROLES))

duplicates = {k: v for k, v in parsed.items() if len(v) > 1}
if duplicates:
    raise RuntimeError("[Fernhall] two markers share a role and yard: %s" % duplicates)

starts = [k for k in parsed if k[0] == "playerstart"]
if len(starts) != 1:
    raise RuntimeError("[Fernhall] a zone needs exactly one player start, found %d (markers: %s)"
                       % (len(starts), sorted(parsed)))

unreal.log("[Fernhall] markers: %s" % sorted("%s@%s" % (r, y or "entry") for (r, y) in parsed))
if len(meshes) != EXPECTED_TOTAL:
    unreal.log_warning("[Fernhall] expected %d static meshes, imported %d"
                       % (EXPECTED_TOTAL, len(meshes)))

for name in sorted(meshes):
    mesh = unreal.load_asset(str(meshes[name].package_name))
    if not mesh:
        raise RuntimeError("[Fernhall] could not load imported mesh %s" % name)
    if name.startswith(SOLID_PREFIXES):
        body = mesh.get_editor_property("body_setup")
        if body:
            body.set_editor_property("collision_trace_flag",
                                     unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    b = mesh.get_bounds()
    o, e = b.origin, b.box_extent
    unreal.log("[Fernhall] %-24s origin (%8.1f, %8.1f, %6.1f)  extent (%7.1f, %7.1f, %6.1f)"
               % (name, o.x, o.y, o.z, e.x, e.y, e.z))

saved = unreal.EditorAssetLibrary.save_directory(DEST, only_if_is_dirty=False, recursive=True)
if not saved:
    raise RuntimeError("[Fernhall] could not save %s" % DEST)
unreal.log("[Fernhall] DONE: %d static meshes in %s" % (len(meshes), DEST))
