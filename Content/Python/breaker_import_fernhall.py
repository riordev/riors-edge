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

It also builds the ground material the composer's slabs wear (O279):
Assets/textures/ground_concrete.png -> T_BreakerGround, sampled by world
position and tinted by a Color parameter, as M_BreakerGround.

Run:
  UnrealEditor-Cmd.exe <project> -run=pythonscript -script="breaker_import_fernhall.py"
"""

import os
import unreal

# THE RUINED TWIN, selected by environment rather than by argv: the runner is
# `-run=pythonscript -script="..."` and what it forwards after the script name
# is version-dependent, while an env var is not.
#
#   set BREAKER_ZONE_TARGET=ruined && UnrealEditor-Cmd.exe ... -script="breaker_import_fernhall.py"
#
# The ruined target imports MESHES ONLY. It creates no map: a rift is not a
# level, it is the existing level built from the other folder, so a second
# Lvl_Fernhall would be a map nothing ever loads.
PROJECT_DIR = unreal.SystemLibrary.get_project_directory()
RUINED = os.environ.get("BREAKER_ZONE_TARGET", "").lower() == "ruined"
GLB = os.path.normpath(os.path.join(PROJECT_DIR, "Assets", "zones",
                                    "fernhall_rift.glb" if RUINED else "fernhall_yard.glb"))
DEST = "/Game/Breaker/Meshes/fernhall_rift" if RUINED else "/Game/Breaker/Meshes/fernhall_yard"
MAP_PACKAGE = "/Game/Breaker/Maps/Lvl_Fernhall"

# The living yard's 323 pieces, plus the ruined target's dressing chunks. The
# ruin is ADDITIVE — same 323 names, the same measured boxes, with dress_ruin_*
# leaned against them — so its total is the yard's plus the debris.
#
# 113 -> 167 when the DEPOT yard landed: a third place, its own seam, its own
# lattice. The ruin's own chunk count follows the cover it leans on, so it grew
# with it rather than being re-authored.
#
# 631 -> 645 (and 715 -> 729) when the patrol return points landed (O274):
# fourteen marker_spawn_* cubes, consumed as transforms like every marker.
EXPECTED_TOTAL = 877 if RUINED else 793
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
# The spawn role carries a 0-based index after the yard (marker_spawn_<n> for
# the entry yard, marker_spawn_<yard>_<n> otherwise — O274). This parser does
# not split the index off: it rides in the yard field, so the (role, yard) key
# below is (spawn, "<yard>_<n>") and the no-repeat rule keeps its shape — two
# spawn markers in one yard with one index are the duplicate, two with
# different indices are not.
#
# LONGEST ROLE FIRST, for the same reason the C++ does it: npc_contract
# contains an underscore, so a shortest-match parse reads marker_npc_contract
# as role "npc" in a yard called "contract".
MARKER_ROLES = ("npc_contract", "playerstart", "rift", "spawn", "yard")


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


# The names the export actually contains. Read from the GLB itself so the prune
# below compares against the SOURCE rather than against whatever happened to be
# in the destination folder.
def read_scene_names(path):
    import json, struct
    with open(path, "rb") as handle:
        data = handle.read()
    # glTF-Binary: 12-byte header, then chunks; the first chunk is the JSON.
    length = struct.unpack_from("<I", data, 12)[0]
    doc = json.loads(data[20:20 + length].decode("utf-8"))
    return set(n.get("name") for n in doc.get("nodes", []) if n.get("name"))

if not os.path.isfile(GLB):
    raise RuntimeError("[Fernhall] source GLB missing: %s (run Scripts/compose_fernhall.py)" % GLB)

# ---- The map. Idempotent, same as breaker_make_levels: never overwrite. ----
if RUINED:
    unreal.log("[Fernhall] ruined target: meshes only, no map.")
elif not unreal.EditorAssetLibrary.does_asset_exist(MAP_PACKAGE):
    level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level_sub.new_level(MAP_PACKAGE):
        raise RuntimeError("[Fernhall] could not create %s" % MAP_PACKAGE)
    unreal.log("[Fernhall] created %s" % MAP_PACKAGE)
else:
    unreal.log("[Fernhall] map exists, untouched: %s" % MAP_PACKAGE)

# ---- The meshes. Reimport in place on re-run. ------------------------------
task = unreal.AssetImportTask()
scene_names = read_scene_names(GLB)
unreal.log("[Fernhall] export contains %d named nodes." % len(scene_names))

task.filename = GLB
task.destination_path = DEST
task.automated = True
task.replace_existing = True
task.save = False  # saved below, after collision settings are applied
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])


# ---- THE GROUND MATERIAL (O279) --------------------------------------------
# A composer slab wears a tiled ground material with a world-aligned grain,
# tinted by role. The grain is Assets/textures/ground_concrete.png, written by
# Scripts/make_ground_texture.py (grey around 0.5, seamless); the material
# samples it by WORLD position, not by the slab's UVs, so two slabs that meet
# share one continuous surface and a slab's scale never stretches the grain.
# Two samples at different scales are blended so the tile period does not
# read as a grid. The role tint is the Color vector parameter — THE SAME NAME
# every paint write uses, so the composer sets it the way it sets any other
# body colour.
#
# Built on every run and rebuilt from empty, like the overlay: idempotent.
# Fenced in try/except because a broken texture import must not block the
# mesh import this script exists for — the yard with a flat ground is still
# a yard; the yard without walls is not.
MATERIALS_DEST = "/Game/Breaker/Materials"
GROUND_TEXTURE_FILE = os.path.normpath(os.path.join(PROJECT_DIR, "Assets", "textures", "ground_concrete.png"))
GROUND_TEXTURE_NAME = "T_BreakerGround"
GROUND_MATERIAL_NAME = "M_BreakerGround"
GROUND_TILE_NEAR_CM = 400.0    # O2 PLACEHOLDER — one texture period in world cm, the fine tile
GROUND_TILE_FAR_CM = 1480.0    # O2 PLACEHOLDER — the coarse tile; non-integer ratio so the two never beat
GROUND_TILE_BLEND = 0.5        # O2 PLACEHOLDER — lerp toward the coarse sample
GROUND_GRAIN_SPREAD = 1.6      # O2 PLACEHOLDER — grey 0..1 maps to this much modulation
GROUND_GRAIN_FLOOR = 0.2       # O2 PLACEHOLDER — ... added to this, so 0.5 grey is 1.0
GROUND_ROUGHNESS = 0.9         # O2 PLACEHOLDER — dry ground, no sheen under the sky


def build_ground_material():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    matlib = unreal.MaterialEditingLibrary

    if not os.path.isfile(GROUND_TEXTURE_FILE):
        raise RuntimeError("source texture missing: %s (run Scripts/make_ground_texture.py)"
                           % GROUND_TEXTURE_FILE)

    # The texture, reimported in place each run so the PNG is the authority.
    tex_task = unreal.AssetImportTask()
    tex_task.filename = GROUND_TEXTURE_FILE
    tex_task.destination_path = MATERIALS_DEST
    tex_task.destination_name = GROUND_TEXTURE_NAME
    tex_task.automated = True
    tex_task.replace_existing = True
    tex_task.save = True
    tools.import_asset_tasks([tex_task])
    texture_path = "%s/%s" % (MATERIALS_DEST, GROUND_TEXTURE_NAME)
    texture = unreal.load_asset(texture_path)
    if not texture:
        raise RuntimeError("texture import produced nothing at %s" % texture_path)
    # Linear grey, not colour: the sample is a modulation factor, and an sRGB
    # read would bend 0.5 grey to ~0.21 and darken every slab.
    texture.set_editor_property("srgb", False)
    try:
        texture.set_editor_property("compression_settings",
                                    unreal.TextureCompressionSettings.TC_GRAYSCALE)
    except Exception as err:  # the enum member is version-dependent; default compression still reads
        unreal.log_warning("[Fernhall] ground texture kept default compression: %s" % err)
    unreal.EditorAssetLibrary.save_asset(texture_path, only_if_is_dirty=False)

    # The material. Load or create, then rebuild the graph from empty.
    material_path = "%s/%s" % (MATERIALS_DEST, GROUND_MATERIAL_NAME)
    material = unreal.load_asset(material_path)
    if not material:
        material = tools.create_asset(GROUND_MATERIAL_NAME, MATERIALS_DEST, unreal.Material,
                                      unreal.MaterialFactoryNew())
    if not material:
        raise RuntimeError("could not create %s" % material_path)
    matlib.delete_all_material_expressions(material)

    def expr(cls, x, y):
        return matlib.create_material_expression(material, cls, x, y)

    # WorldPosition.xy / period -> UV. Two periods, blended.
    world = expr(unreal.MaterialExpressionWorldPosition, -1400, 0)
    mask = expr(unreal.MaterialExpressionComponentMask, -1200, 0)
    mask.set_editor_property("r", True)
    mask.set_editor_property("g", True)
    mask.set_editor_property("b", False)
    mask.set_editor_property("a", False)
    matlib.connect_material_expressions(world, "", mask, "")

    def sample_at(period_cm, y):
        divide = expr(unreal.MaterialExpressionDivide, -1000, y)
        divide.set_editor_property("const_b", period_cm)
        matlib.connect_material_expressions(mask, "", divide, "A")
        sample = expr(unreal.MaterialExpressionTextureSample, -800, y)
        sample.set_editor_property("texture", texture)
        matlib.connect_material_expressions(divide, "", sample, "UVs")
        return sample

    near = sample_at(GROUND_TILE_NEAR_CM, -100)
    far = sample_at(GROUND_TILE_FAR_CM, 200)
    blend = expr(unreal.MaterialExpressionLinearInterpolate, -600, 0)
    blend.set_editor_property("const_alpha", GROUND_TILE_BLEND)
    matlib.connect_material_expressions(near, "R", blend, "A")
    matlib.connect_material_expressions(far, "R", blend, "B")

    # grain = grey * spread + floor: mid-grey is exactly 1.0, so the tint is
    # the slab's colour and the texture only breaks it up.
    spread = expr(unreal.MaterialExpressionConstant, -600, 150)
    spread.set_editor_property("r", GROUND_GRAIN_SPREAD)
    scaled = expr(unreal.MaterialExpressionMultiply, -450, 0)
    matlib.connect_material_expressions(blend, "", scaled, "A")
    matlib.connect_material_expressions(spread, "", scaled, "B")
    floor = expr(unreal.MaterialExpressionConstant, -450, 150)
    floor.set_editor_property("r", GROUND_GRAIN_FLOOR)
    grain = expr(unreal.MaterialExpressionAdd, -300, 0)
    matlib.connect_material_expressions(scaled, "", grain, "A")
    matlib.connect_material_expressions(floor, "", grain, "B")

    color = expr(unreal.MaterialExpressionVectorParameter, -300, 200)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    tinted = expr(unreal.MaterialExpressionMultiply, -150, 0)
    matlib.connect_material_expressions(grain, "", tinted, "A")
    matlib.connect_material_expressions(color, "", tinted, "B")
    matlib.connect_material_property(tinted, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rough = expr(unreal.MaterialExpressionConstant, -150, 300)
    rough.set_editor_property("r", GROUND_ROUGHNESS)
    matlib.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    matlib.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_asset(material_path, only_if_is_dirty=False):
        raise RuntimeError("could not save %s" % material_path)
    unreal.log("[Fernhall] ground material: %s samples %s" % (material_path, texture_path))


try:
    build_ground_material()
except Exception as err:
    unreal.log_warning("[Fernhall] ground material NOT built, mesh import continues: %s" % err)

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

# ---- PRUNE WHAT THE EXPORT NO LONGER CONTAINS ------------------------------
# A re-import ADDS and OVERWRITES; it does not remove. So a piece that was
# RENAMED or SPLIT leaves its old asset behind, and UBreakerZoneBuilder spawns
# everything in this folder — which means the stale mesh still stands in the
# world. Splitting wall_e into wall_e_s and wall_e_n left the original slab
# sitting across the seam's mouth, silently closing a connection that had just
# been authored. The count check below noticed; nothing else would have.
#
# The GLB is the authority: this folder is its projection, not a place assets
# accumulate.
exported = set(scene_names)
stale = sorted(n for n in meshes if n not in exported)
for name in stale:
    package = str(meshes[name].package_name)
    unreal.log_warning("[Fernhall] pruning %s: no longer in the export." % name)
    unreal.EditorAssetLibrary.delete_asset(package)
if stale:
    meshes = {n: d for n, d in meshes.items() if n not in stale}
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
