# The Fernhall approach yard — the vertical slice's one zone, AUTHORED HERE.
#
#   python Scripts/compose_fernhall.py
#
# Reads the CC0 kit pieces in Assets/zones/kit (Kenney starter kits, models
# CC0 — see Assets/zones/kit/LICENSE-NOTE.txt) and writes
# Assets/zones/fernhall_yard.glb: one scene, every instance a separately named
# mesh with its WORLD transform baked into the vertices. That bake is the
# whole pipeline contract — the importer splits the scene into per-name static
# meshes whose local bounds sit at their world positions, so spawning every
# mesh at the identity transform recovers the scene (the anchor_hub route),
# and the same recovered bounds are what the grammar validator measures.
#
# THE NAME PREFIX IS THE CONTRACT, read by UBreakerZoneBuilder and by the
# grammar test:
#   blk_full_*    full-height cover          — collides, measured (FullHeight)
#   blk_chest_*   chest-high cover           — collides, measured (ChestHigh)
#   wall_*        perimeter boundary         — collides, NOT measured: the walls
#                 bound the field, they do not stand in it, and two adjacent
#                 perimeter slabs touching would read as an illegal 0 cm gap
#   flr_*         ground                     — collides, not cover
#   dress_*       set dressing               — no collision, not measured
#   marker_*      consumed as a transform, never rendered
#
# SIZES ARE THE GRAMMAR'S, not the kit's: pieces are scaled to the cover
# registry's authored dimensions (chest 300x120x120 cm, full-height 300x400)
# so the placed yard speaks the same vocabulary the generator does. The
# validated combat band is X 20..95 m — the entry plaza and the rift pad are
# deliberately open ground, the same exclusion the gym's instrument corridor
# claims.
#
# Authored in glTF space: X forward (toward the rift), Y up, Z lateral.
import os
import numpy as np
import trimesh

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
KIT = os.path.join(ROOT, "Assets", "zones", "kit")
OUT = os.path.join(ROOT, "Assets", "zones", "fernhall_yard.glb")

def load_piece(name):
    path = os.path.join(KIT, name)
    loaded = trimesh.load(path, force="mesh")
    return loaded

PIECES = {
    "chest": load_piece("fps_wall-low.glb"),
    "full": load_piece("fps_wall-high.glb"),
    "bldg_a": load_piece("city_building-small-a.glb"),
    "bldg_b": load_piece("city_building-small-b.glb"),
    "bldg_c": load_piece("city_building-small-c.glb"),
    "garage": load_piece("city_building-garage.glb"),
    "pavement": load_piece("city_pavement.glb"),
    "trees": load_piece("city_grass-trees.glb"),
    "grass": load_piece("fps_grass.glb"),
    "mound": load_piece("fps_platform-large-grass.glb"),
}

SCENE = {}

def place(out_name, piece_key, at, target_size=None, marker=False):
    """Bake one instance: scale the piece to target_size metres (if given),
    ground it (min-Y to 0), translate to `at` (x, z lateral) and register
    under its contract name."""
    if marker:
        mesh = trimesh.creation.box(extents=(0.5, 0.5, 0.5))
    else:
        mesh = PIECES[piece_key].copy()
    bounds = mesh.bounds
    size = bounds[1] - bounds[0]
    if target_size is not None:
        scale = np.array([target_size[0] / size[0], target_size[1] / size[1], target_size[2] / size[2]])
        mesh.apply_scale(scale)
        bounds = mesh.bounds
    # Centre on X/Z, floor to Y=0, then translate.
    centre = (bounds[0] + bounds[1]) * 0.5
    mesh.apply_translation((-centre[0], -bounds[0][1], -centre[2]))
    mesh.apply_translation((at[0], at[1] if len(at) > 2 else 0.0, at[-1]))
    assert out_name not in SCENE, out_name
    SCENE[out_name] = mesh

# ---- Ground: the yard slab and the rift pad --------------------------------
place("flr_yard", "pavement", (51.0, -0.06, 0.0), (106.0, 0.06, 56.0))
place("flr_riftpad", "pavement", (92.0, 0.0, 0.0), (10.0, 0.08, 10.0))

# ---- Perimeter: building slabs on both flanks and both ends ----------------
BLDG = ["bldg_a", "bldg_b", "bldg_c", "garage"]
for i, x in enumerate(range(5, 100, 10)):
    place("wall_n%02d" % i, BLDG[i % 4], (float(x), 0.0, 25.0), (10.0, 7.0, 3.0))
    place("wall_s%02d" % i, BLDG[(i + 2) % 4], (float(x), 0.0, -25.0), (10.0, 7.0, 3.0))
place("wall_w", "bldg_b", (-1.5, 0.0, 0.0), (3.0, 7.0, 56.0))
# THE EAST WALL HAS A MOUTH IN IT. Two stubs rather than one slab, leaving a
# 10 m gap on the z 9..19 band: that gap is the entry yard's end of the seam to
# the SUBSTATION yard. Mouth width is a CEILING in the connection rule (a seam
# is recognisable because it narrows), so 10 m sits under the 12 m cap with room
# for the cap to come down after someone walks it.
place("wall_e_s", "bldg_c", (101.5, 0.0, -9.5), (3.0, 7.0, 37.0))
place("wall_e_n", "bldg_c", (101.5, 0.0, 23.5), (3.0, 7.0, 9.0))

# ---- The cover lattice ------------------------------------------------------
# TWO DIFFERENT LANES LIVE HERE AND THEY ARE GUARDED BY DIFFERENT RULES.
#
# Chest pairs flank the main lane every 15 m (pitch under the 17 m grammar
# max), standing on the corridor's 10.5 m shoulder line; their inner faces
# sit at +-9.9 m, so the ground between them is 19.8 m wide. THAT LANE IS
# GUARDED BY THE CORRIDOR RULE, not by the dash-corridor floor: no cover of
# any class may stand within CorridorHalfWidth (9 m) of the centreline, so
# pulling these pairs in to +-5 m goes RED — see O132 and the
# RiorsEdge.Zone.Fernhall.LaneGuard perturbation. The 19.8 m figure is this
# file's own arithmetic off the 10.5 m shoulder; the build prints the
# SHOULDER OFFSET against its floor, because a width has to pick
# centre-to-centre or face-to-face and an offset does not.
#
# The dash-corridor floor guards the OTHER lane, the full-height one. Line
# breaks stand off-lane, each its own cluster, spaced so the clear gap
# between any two stays over the 16 m floor — 24 m centre-to-centre is the
# tightest pair here, 21 m clear after extents. Chest cover is invisible to
# that measurement by design, which is why it needs the corridor rule.
for i, x in enumerate((25.0, 40.0, 55.0, 70.0, 85.0)):
    place("blk_chest_n%02d" % i, "chest", (x, 0.0, 10.5), (3.0, 1.2, 1.2))
    place("blk_chest_s%02d" % i, "chest", (x, 0.0, -10.5), (3.0, 1.2, 1.2))
for i, (x, z) in enumerate(((32.0, 17.0), (32.0, -17.0), (62.0, 17.0), (62.0, -17.0), (86.0, 17.0), (89.0, -18.0))):
    place("blk_full_break%02d" % i, "full", (x, 0.0, z), (3.0, 4.0, 3.0))

# ---- THE SEAM: entry yard -> substation yard --------------------------------
# A CONNECTION IS A DISTINCT KIND OF SPACE and this is the first one. Every term
# is the opposite shape to a yard's: the mouth is a CEILING rather than a floor,
# the length is a CEILING because a corridor long enough to walk is a place, and
# it TURNS — no through-sight, because O1 makes movement the only active
# defence and a straight seam lets a ranged enemy in the far yard hold a player
# whose cover was never laid for that angle.
#
# The dog-leg is the no-through-sight term made of geometry: east 15 m, then
# north 14 m. 29 m walked, under the 30 m ceiling, and nothing at either mouth
# can see the other.
place("flr_seam_a", "pavement", (108.5, -0.06, 14.0), (15.0, 0.06, 10.0))
place("flr_seam_b", "pavement", (111.0, -0.06, 26.0), (10.0, 0.06, 14.0))
# Seam walls. The OUTSIDE of the corner is what blocks the sightline.
place("wall_seam_s", "garage", (108.5, 0.0, 8.5), (15.0, 7.0, 3.0))
place("wall_seam_e", "garage", (117.5, 0.0, 20.5), (3.0, 7.0, 23.0))
place("wall_seam_w", "garage", (104.5, 0.0, 20.5), (3.0, 7.0, 3.0))
place("wall_seam_wn", "garage", (104.5, 0.0, 26.0), (3.0, 7.0, 14.0))

# ---- The SUBSTATION yard, the second place in the world ---------------------
# Same footprint as the entry yard, because sizes stay near the walked one until
# one has been walked. Its own frame, its own lattice, its own rift door — the
# marker role exists so a yard can have all three.
SUB_X, SUB_Z = 111.0, 61.0   # centre
place("flr_yard_sub", "pavement", (SUB_X, -0.06, SUB_Z), (106.0, 0.06, 56.0))
place("flr_riftpad_sub", "pavement", (SUB_X + 41.0, 0.0, SUB_Z), (10.0, 0.08, 10.0))

# Perimeter, with a mouth on the south flank where the seam arrives (x 106..116).
for i, x in enumerate(range(int(SUB_X) - 46, int(SUB_X) + 50, 10)):
    place("wall_sub_n%02d" % i, BLDG[i % 4], (float(x), 0.0, SUB_Z + 25.0), (10.0, 7.0, 3.0))
    if 106.0 <= float(x) <= 116.0:
        continue   # the seam's far mouth
    place("wall_sub_s%02d" % i, BLDG[(i + 2) % 4], (float(x), 0.0, SUB_Z - 25.0), (10.0, 7.0, 3.0))
place("wall_sub_w", "bldg_b", (SUB_X - 53.5, 0.0, SUB_Z), (3.0, 7.0, 56.0))
place("wall_sub_e", "bldg_c", (SUB_X + 53.5, 0.0, SUB_Z), (3.0, 7.0, 56.0))

# The same lattice, in this yard's own frame. Chest pairs on the corridor
# shoulder, full-height line breaks off-lane, spacing unchanged from the yard
# the grammar was measured against.
# OFFSETS ARE MEASURED FROM THIS YARD'S ANCHOR, matching the entry yard's
# layout exactly in its own frame. The first attempt eyeballed them from the
# yard CENTRE instead and landed the lattice 4 m short: same sixteen pieces,
# same spacing, but more uncovered ground at the far edge, and the yard failed
# the exposed-crossing rule at 1750 against a 1700 ceiling while the entry yard
# read 1450. A second yard authored from a validated first one has to copy the
# frame-relative numbers, not the shape by eye.
SUB_ANCHOR = SUB_X - 41.0
for i, fwd in enumerate((19.0, 34.0, 49.0, 64.0, 79.0)):
    place("blk_chest_sub_n%02d" % i, "chest", (SUB_ANCHOR + fwd, 0.0, SUB_Z + 10.5), (3.0, 1.2, 1.2))
    place("blk_chest_sub_s%02d" % i, "chest", (SUB_ANCHOR + fwd, 0.0, SUB_Z - 10.5), (3.0, 1.2, 1.2))
for i, (fwd, dz) in enumerate(((26.0, 17.0), (26.0, -17.0), (56.0, 17.0), (56.0, -17.0), (80.0, 17.0), (83.0, -18.0))):
    place("blk_full_sub_break%02d" % i, "full", (SUB_ANCHOR + fwd, 0.0, SUB_Z + dz), (3.0, 4.0, 3.0))

# ---- Markers ----------------------------------------------------------------
# THE NAME CARRIES A ROLE AND A YARD, and this is the authoring side of a
# contract with two readers — BreakerZoneBuilder::ParseMarkerName and
# breaker_import_fernhall.py's parse_marker. Change it here and both refuse
# the export rather than importing a zone that is quietly missing something.
#
#   marker_<role>          this marker belongs to the ENTRY yard
#   marker_<role>_<yard>   it belongs to <yard>
#
# Roles: playerstart, rift, npc_contract, yard. An unknown role is REFUSED, not
# skipped, so a typo here is a loud failure rather than a marker that silently
# does not exist.
#
# EXACTLY ONE playerstart per zone. Rift doors and contract givers are
# per-yard and OPTIONAL — a yard with no door is a legal yard — but no
# (role, yard) pair may repeat: two rift markers in one yard would spawn two
# doors on the same spot.
#
# EVERY NAMED YARD NEEDS A `yard` ANCHOR. A yard's grammar is measured in its
# OWN frame, and a zone has exactly one playerstart, so the rule that anchors
# the entry yard cannot anchor a second: marker_yard_<name> is what gives yard
# <name> a frame. The ENTRY yard is exempt because the playerstart anchors it.
#
# The three below carry no yard suffix because Fernhall is one yard today.
# Growing it means adding marker_yard_<name> plus that yard's own markers, not
# changing any of this.
place("marker_playerstart", None, (6.0, 0.0, 0.0), marker=True)
place("marker_rift", None, (92.0, 0.0, 0.0), marker=True)
place("marker_npc_contract", None, (13.0, 0.0, -14.0), marker=True)

# The SUBSTATION yard's anchor and its own rift door. The anchor is what gives
# this yard a frame: a zone has exactly one playerstart, so nothing else could.
# Its forward is derived the same way the entry yard's is — from what it points
# at, which is its own rift.
place("marker_yard_substation", None, (SUB_X - 41.0, 0.0, SUB_Z), marker=True)
place("marker_rift_substation", None, (SUB_X + 41.0, 0.0, SUB_Z), marker=True)

# ---- Dressing (O24: vegetation over ruins) ---------------------------------
for i, (x, z) in enumerate(((18.0, 20.0), (35.0, -21.0), (50.0, 21.0), (68.0, -20.0), (88.0, 20.0), (10.0, -20.0))):
    place("dress_trees%02d" % i, "trees", (x, 0.0, z), (6.0, 4.0, 4.0))
place("dress_mound", "mound", (47.5, 0.0, -19.0), (8.0, 0.8, 8.0))
for i, (dx, dz) in enumerate(((-33.0, 20.0), (-16.0, -21.0), (1.0, 21.0), (19.0, -20.0), (39.0, 20.0), (-41.0, -20.0))):
    place("dress_trees_sub%02d" % i, "trees", (SUB_X + dx, 0.0, SUB_Z + dz), (6.0, 4.0, 4.0))
place("dress_mound_sub", "mound", (SUB_X - 3.5, 0.0, SUB_Z - 19.0), (8.0, 0.8, 8.0))
for i, (x, z) in enumerate(((22.0, 4.0), (30.0, -7.0), (44.0, 6.0), (58.0, -4.0), (73.0, 7.0), (81.0, -6.0), (15.0, 9.0), (90.0, -8.0))):
    place("dress_grass%02d" % i, "grass", (x, 0.0, z))

scene = trimesh.Scene(SCENE)
os.makedirs(os.path.dirname(OUT), exist_ok=True)
scene.export(OUT)
print("wrote", OUT, "meshes:", len(SCENE))
