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
#
# THE RUINED TARGET (--ruined) writes Assets/zones/fernhall_rift.glb instead:
# the SAME layout under the SAME names, with the cover pieces swapped for the
# megakit's broken twins. Same names because the whole consumer chain keys off
# them — the markers, the courtyard plan, the local map and the grammar test
# all read a piece by its name, so a rift built from this file is the same yard
# to every one of them and a different PLACE to look at.
#
# RUIN IS ADDED, NOT SWAPPED, AND THE FRAMES ARE WHY. The first version of this
# swapped the cover pieces themselves for the megakit's broken twins, which is
# safe — place() forces every piece to the cover registry's authored dimensions,
# so the measured box cannot move — and photographing it showed the swap was
# also INVISIBLE. A broken wall is 0.68 x 3.01 x 4.82 m; squashed into a
# 3.0 x 1.2 x 1.2 chest-high box the break is scaled out of existence, and the
# two targets rendered identically. The forced size is exactly what made the
# swap safe and exactly what made it null.
#
# So the cover stays the cover, at its authored box, and the ruin arrives as
# dress_ruin_* — broken chunks at their OWN proportions, rotated, leaned
# against the cover and along the perimeter. dress_* carries no collision and
# is not measured, so the fight is untouched and the silhouette is not.
#
# PERIMETER BUILDINGS ARE NOT TOUCHED EITHER. wall_* pieces BOUND the field,
# and a broken perimeter is a hole a player walks out of.
import os
import sys
import numpy as np
import trimesh

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
KIT = os.path.join(ROOT, "Assets", "zones", "kit")
RUINED = "--ruined" in sys.argv
OUT = os.path.join(ROOT, "Assets", "zones",
                   "fernhall_rift.glb" if RUINED else "fernhall_yard.glb")

def load_piece(name):
    path = os.path.join(KIT, name)
    loaded = trimesh.load(path, force="mesh")
    return loaded

# The broken twins, from the sci-fi megakit that was materialised for exactly
# this and used by nothing until now. Two of them, so chest and full-height
# cover do not read as the same object at two scales.
BROKEN_CHEST = "modular-sci-fi-megakit/glTF/Walls/WallBand_Straight_Broken.gltf"
BROKEN_FULL = "modular-sci-fi-megakit/glTF/Walls/WallAstra_Straight_Broken.gltf"

PIECES = {
    "chest": load_piece("fps_wall-low.glb"),
    "full": load_piece("fps_wall-high.glb"),
    "ruin_chunk": load_piece(BROKEN_CHEST),
    "ruin_slab": load_piece(BROKEN_FULL),
    "bldg_a": load_piece("city_building-small-a.glb"),
    "bldg_b": load_piece("city_building-small-b.glb"),
    "bldg_c": load_piece("city_building-small-c.glb"),
    "garage": load_piece("city_building-garage.glb"),
    "pavement": load_piece("city_pavement.glb"),
    "trees": load_piece("city_grass-trees.glb"),
    "grass": load_piece("fps_grass.glb"),
    "mound": load_piece("fps_platform-large-grass.glb"),
    # THE MEGAKIT'S OWN VOCABULARY, and it was already in the repo. Owner:
    # "there's some graphical assets that are imported that are placed
    # randomly, like trees, but then everything else is just grey box". A yard
    # made of scaled city blocks and two wall prototypes has nothing in it that
    # says what the place DOES; these are the pieces that do.
    "crate": load_piece("modular-sci-fi-megakit/glTF/Props/Prop_Crate4.gltf"),
    "barrel": load_piece("modular-sci-fi-megakit/glTF/Props/Prop_Barrel_Large.gltf"),
    "cable": load_piece("modular-sci-fi-megakit/glTF/Props/Prop_Cable_3.gltf"),
    "panel": load_piece("modular-sci-fi-megakit/glTF/Walls/WallAstra_Straight.gltf"),
    "stairs": load_piece("modular-sci-fi-megakit/glTF/Platforms/Platform_Stairs_4Wide.gltf"),
    "rails": load_piece("modular-sci-fi-megakit/glTF/Platforms/Platform_Rails_4Wide.gltf"),
    "column": load_piece("modular-sci-fi-megakit/glTF/Columns/Column_MetalSupport.gltf"),
}

SCENE = {}

def place(out_name, piece_key, at, target_size=None, marker=False, yaw=0.0, lean=0.0):
    """Bake one instance: scale the piece to target_size metres (if given),
    ground it (min-Y to 0), translate to `at` (x, z lateral) and register
    under its contract name.

    yaw/lean rotate the piece about the up axis and about X before it is
    grounded. Only the ruin dressing uses them: a field of rubble all facing
    the same way reads as a row of props, not as collapse. They are applied
    BEFORE the ground-and-translate below, so a leaned chunk still sits on the
    floor rather than hovering or sinking."""
    if marker:
        mesh = trimesh.creation.box(extents=(0.5, 0.5, 0.5))
    else:
        mesh = PIECES[piece_key].copy()
    if yaw or lean:
        mesh.apply_transform(trimesh.transformations.rotation_matrix(np.radians(yaw), (0, 1, 0)))
        mesh.apply_transform(trimesh.transformations.rotation_matrix(np.radians(lean), (1, 0, 0)))
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

# EVERY PERIMETER PIECE WAS THE SAME BOX. Ten of them a side, 10 x 7 x 3 m each,
# at one distance, in one line: owner, "awkwardly generated not really buildings
# on the left right front and back". A boundary made of one repeated box is a
# fence, and a fence is what makes a yard read as a rectangle however much is
# built inside it.
#
# So the boundary gets a SKYLINE: five profiles walked in order, differing in
# height, in depth, and in how far back from the line they stand, with a smaller
# second mass on some of them. Nothing here is measured by the cover grammar
# (wall_ bounds the field, it does not stand in it), so this is silhouette work
# and cannot move a single cover number.
#
#   height, depth, setback (+ is away from the yard), second mass or None
SKYLINE = (
    (7.0,  3.0, 0.0, None),
    (11.5, 4.0, 0.8, ("tower", 3.2, 4.5)),
    (5.5,  5.5, 2.2, ("canopy", 7.0, 0.5)),
    (9.0,  3.0, 0.4, ("stack", 2.0, 3.0)),
    (6.5,  6.0, 1.4, None),
)

def perimeter(tag, x, z, index, facing):
    """One perimeter building on the line z, facing the yard (facing = -1 when
    the yard is at lower z). The profile is chosen by index so the two flanks of
    one yard never step in time with each other."""
    height, depth, setback, second = SKYLINE[index % len(SKYLINE)]
    # THE INNER FACE DOES NOT MOVE. Depth and setback grow AWAY from the yard,
    # never into it: the old slab was 3 m deep on this line, so its inner face
    # stood 1.5 m in from the centre, and that face is the boundary every other
    # thing in this yard was authored against. The first version let a profile
    # push toward the lane and a 5.5 m-deep building swallowed the courtyard
    # route's walking line at z -21.3 — RiorsEdge.World.Fernhall.CourtyardRoute
    # caught it, which is exactly the measurement that route probe is for.
    inner = z + facing * 1.5
    line = inner - facing * (depth * 0.5 + max(0.0, setback))
    place("wall_%s%02d" % (tag, index), BLDG[index % 4], (x, 0.0, line), (10.0, height, depth))
    if second is None:
        return
    kind, size, rise = second
    if kind == "tower":
        # A stair tower or a lift head: narrow, taller than its parent, and set
        # to one side so the parent's roofline breaks rather than steps.
        place("wall_%s%02dt" % (tag, index), "bldg_c", (x + 3.0, 0.0, line + facing * 0.6),
              (size, height + rise, depth * 0.7))
    elif kind == "canopy":
        # A loading canopy over the ground in front of the building, on two
        # columns. This is the piece that makes a wall read as somewhere goods
        # came in and out of.
        # Out over the ground in FRONT of the building, from the inner face and
        # overhead: a canopy is the one profile that reaches toward the yard,
        # and it does it at 3.9 m where nothing walks.
        place("flr_%s%02dc" % (tag, index), "pavement", (x, rise + 3.4, inner + facing * 1.4),
              (size, 0.35, 5.0))
        for side, dx in enumerate((-2.6, 2.6)):
            place("dress_%s%02dcol%d" % (tag, index, side), "column",
                  (x + dx, 0.0, inner + facing * 3.4), (0.5, 3.4, 0.5))
    elif kind == "stack":
        # A vent stack on the roof: no footprint of its own, all silhouette.
        place("dress_%s%02ds" % (tag, index), "column", (x - 2.5, height, line),
              (size * 0.5, rise, size * 0.5))

for i, x in enumerate(range(5, 100, 10)):
    perimeter("n", float(x), 25.0, i, -1.0)
    # THE SOUTH FLANK HAS A MOUTH IN IT TOO (O276): the piece at x 5 is left
    # out, opening x 0..10 on the plaza's west flank as the third seam's near
    # end. A perimeter piece is 10 m wide centred on the loop's x, so the mouth
    # a skip opens is 0..10 or 10..20, never 5..15; 0..10 is the one that lies
    # wholly inside the plaza (x < 14, outside the 20..95 combat band) and
    # shares wall_w's line, so the seam's west wall simply carries it south.
    # 10 m sits under the 12 m mouth ceiling like the east mouth does.
    if x == 5:
        continue
    perimeter("s", float(x), -25.0, i + 2, 1.0)
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
# Seam slabs ABUT the yard slabs; they do not lie on them. Every floor here
# shares one top face (Y=0), and a seam that started at the wall's inner face
# ran 3 m across the yard slab that runs 4.5 m past the perimeter line — two
# floors on one plane, painted different colours, is the flicker he walked over.
place("flr_seam_a", "pavement", (110.0, -0.06, 14.0), (12.0, 0.06, 10.0))
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
    # The NORTH flank now has a mouth of its own at x 130..140: this yard is no
    # longer the end of the world, and the second seam leaves through it.
    if not 130.0 <= float(x) <= 140.0:
        perimeter("sub_n", float(x), SUB_Z + 25.0, i + 1, -1.0)
    if 106.0 <= float(x) <= 116.0:
        continue   # the seam's far mouth
    perimeter("sub_s", float(x), SUB_Z - 25.0, i + 3, 1.0)
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

# ---- THE SECOND SEAM: substation -> depot ------------------------------------
# Same rule as the first, and the same shape read off it rather than copied by
# eye: the mouth is a CEILING (10 m, under the 12 m cap), the walk is a CEILING
# (28 m, under 30), and it TURNS so neither mouth can see the other. It leaves
# through the substation's NORTH flank and turns EAST, away from the entry yard,
# which is what keeps three yards on one plane without any of them overlapping.
place("flr_seam2_a", "pavement", (135.0, -0.06, 92.0), (10.0, 0.06, 6.0))
place("flr_seam2_b", "pavement", (141.75, -0.06, 100.0), (23.5, 0.06, 10.0))
# THE OUTSIDE OF THE CORNER IS WHAT BLOCKS THE SIGHTLINE. The wall that matters
# is the third one: without it a body standing at the depot mouth can see
# diagonally across the corner to the substation mouth, which is the exact
# through-sight the dog-leg exists to remove.
place("wall_seam2_w", "garage", (128.5, 0.0, 90.0), (3.0, 7.0, 10.0))
place("wall_seam2_wn", "garage", (128.5, 0.0, 100.0), (3.0, 7.0, 10.0))
place("wall_seam2_corner", "garage", (146.5, 0.0, 93.5), (13.0, 7.0, 3.0))
place("wall_seam2_e", "garage", (141.5, 0.0, 88.5), (3.0, 7.0, 7.0))
place("wall_seam2_n", "garage", (142.0, 0.0, 106.5), (24.0, 7.0, 3.0))

# ---- The DEPOT yard, the third place in the world ---------------------------
# Owner-asked: "expand the size a lot as well and add more pockets". Fernhall
# was two yards and a seam, and a third is what the file already said growth
# looks like — a yard anchor plus that yard's own markers, changing nothing
# else. Same 106 x 56 footprint and the same frame-relative lattice, because
# the substation's own note records what happens when a second yard is authored
# by eye instead: identical pieces, 4 m short, and a failed exposed-crossing
# measurement the first yard passed.
#
# NO RIFT DOOR, DELIBERATELY. A yard with no door is a legal yard, and a second
# door into the substation undercroft would be two ways into one place. With no
# rift to point at, the frame keeps +X — which is the direction the player is
# already walking when they come out of the seam.
DEP_X, DEP_Z = 206.5, 120.0   # centre
DEP_ANCHOR = DEP_X - 41.0
place("flr_yard_dep", "pavement", (DEP_X, -0.06, DEP_Z), (106.0, 0.06, 56.0))

# Perimeter. The mouth is in the WEST flank, where the seam arrives, so the
# west wall is two stubs and the other three sides are solid.
place("wall_dep_w_s", "bldg_b", (DEP_X - 53.5, 0.0, DEP_Z - 26.5), (3.0, 7.0, 3.0))
place("wall_dep_w_n", "bldg_b", (DEP_X - 53.5, 0.0, DEP_Z + 6.5), (3.0, 7.0, 43.0))
place("wall_dep_e", "bldg_c", (DEP_X + 53.5, 0.0, DEP_Z), (3.0, 7.0, 56.0))
for i, x in enumerate(range(int(DEP_X) - 46, int(DEP_X) + 50, 10)):
    perimeter("dep_n", float(x), DEP_Z + 25.0, i + 4, -1.0)
    perimeter("dep_s", float(x), DEP_Z - 25.0, i, 1.0)

# The lattice, frame-relative and unchanged.
for i, fwd in enumerate((19.0, 34.0, 49.0, 64.0, 79.0)):
    place("blk_chest_dep_n%02d" % i, "chest", (DEP_ANCHOR + fwd, 0.0, DEP_Z + 10.5), (3.0, 1.2, 1.2))
    place("blk_chest_dep_s%02d" % i, "chest", (DEP_ANCHOR + fwd, 0.0, DEP_Z - 10.5), (3.0, 1.2, 1.2))
for i, (fwd, dz) in enumerate(((26.0, 17.0), (26.0, -17.0), (56.0, 17.0), (56.0, -17.0), (80.0, 17.0), (83.0, -18.0))):
    place("blk_full_dep_break%02d" % i, "full", (DEP_ANCHOR + fwd, 0.0, DEP_Z + dz), (3.0, 4.0, 3.0))

# ---- THE THIRD SEAM: entry plaza -> siding (O276) ---------------------------
# Out of the plaza's WEST flank (glTF -Z: the survey draws UE +X as north) and
# the same shape as the first two, read off the rule rather than copied by eye:
# a 10 m mouth under the 12 m ceiling, and it TURNS. South 15 m from the plaza
# slab's edge, then west 14 m to the siding's end wall: 29 m walked, under 30,
# and the same 29 the zone builder's connection table honours.
#
# Seam slabs ABUT: leg a's north edge is the entry slab's edge at z -28, leg b's
# north edge is leg a's south edge, and leg b's west end is the siding slab's
# east edge at x -4 — FloorsDisjoint measures every pair.
place("flr_seam3_a", "pavement", (5.0, -0.06, -35.5), (10.0, 0.06, 15.0))    # x 0..10, z -43..-28
place("flr_seam3_b", "pavement", (3.0, -0.06, -48.0), (14.0, 0.06, 10.0))    # x -4..10, z -53..-43
# Seam walls, on the OUTSIDE of the corner. The west wall carries wall_w's line
# south to the siding's north-east stub; the east wall runs from behind the
# plaza's next perimeter piece (overlapping it, so the metre between that
# building's back and the slab edge is not a pocket a body can slip into) down
# to the south wall, and the south wall closes the corner from the siding's
# south-east stub across to the east wall. Looking south from the plaza mouth
# ends at the south wall; looking east from the siding mouth ends at the east
# wall. The corner-to-corner diagonal is the same sliver the first two seams
# leave, and no wider.
place("wall_seam3_w", "garage", (-1.5, 0.0, -35.5), (3.0, 7.0, 15.0))        # x -3..0, z -43..-28
place("wall_seam3_e", "garage", (11.5, 0.0, -40.0), (3.0, 7.0, 32.0))        # x 10..13, z -56..-24
place("wall_seam3_s", "garage", (5.5, 0.0, -54.5), (15.0, 7.0, 3.0))         # x -2..13, z -56..-53

# ---- The SIDING yard, the fourth place in the world (O276) -------------------
# Off the entry plaza's west flank, one seam from the player start, with its
# own door and its own level beside the entry yard's. Same 106 x 56 footprint
# and the same frame-relative lattice, for the reason the substation's note
# records.
#
# IT FACES -X, AND THAT IS THE ONLY THING NEW ABOUT ITS FRAME. The seam's west
# leg ends at a constant-x line, so the yard it enters presents an END WALL
# there, the way the depot does at its west end; the mouth is at the yard's
# anchor end so the walk in runs down the lane toward the rift, not out of it.
# The anchor is therefore the EAST end (centre + 41) and the rift the west
# (centre - 41): rift = anchor + forward x 82, forward = -X, and YardFrame reads
# that from the two markers the same way it reads +X off the others. Every
# frame-relative number below is the substation's mirrored: fwd is subtracted
# from the anchor instead of added, and lat stays absolute glTF z. Measured in
# the yard's own frame that is the substation's lattice reflected across the
# lane, and every rule the grammar applies is symmetric across it.
SID_X, SID_Z = -57.0, -66.0   # centre; slab x -110..-4, z -94..-38
SID_ANCHOR = SID_X + 41.0     # x -16; the rift at SID_X - 41 = x -98
place("flr_yard_siding", "pavement", (SID_X, -0.06, SID_Z), (106.0, 0.06, 56.0))
place("flr_riftpad_siding", "pavement", (SID_X - 41.0, 0.0, SID_Z), (10.0, 0.08, 10.0))

# Perimeter. Both flanks are solid; the mouth is in the EAST end wall, where
# the seam arrives, so the east wall is two stubs about the z -53..-43 band
# (lat +13..+23) and the west wall is one slab. Eleven flank pieces rather than
# ten, so the row reaches the end walls' outer faces and leaves no open corner
# at the slab's edge.
for i, x in enumerate(range(int(SID_X) - 50, int(SID_X) + 51, 10)):
    perimeter("siding_n", float(x), SID_Z + 25.0, i, -1.0)
    perimeter("siding_s", float(x), SID_Z - 25.0, i + 3, 1.0)
place("wall_siding_w", "bldg_b", (SID_X - 53.5, 0.0, SID_Z), (3.0, 7.0, 56.0))
place("wall_siding_e_n", "bldg_c", (SID_X + 53.5, 0.0, SID_Z + 25.5), (3.0, 7.0, 5.0))    # z -43..-38
place("wall_siding_e_s", "bldg_c", (SID_X + 53.5, 0.0, SID_Z - 7.5), (3.0, 7.0, 41.0))    # z -94..-53

# The lattice, frame-relative and unchanged: the same offsets from the anchor,
# walked toward -X.
for i, fwd in enumerate((19.0, 34.0, 49.0, 64.0, 79.0)):
    place("blk_chest_siding_n%02d" % i, "chest", (SID_ANCHOR - fwd, 0.0, SID_Z + 10.5), (3.0, 1.2, 1.2))
    place("blk_chest_siding_s%02d" % i, "chest", (SID_ANCHOR - fwd, 0.0, SID_Z - 10.5), (3.0, 1.2, 1.2))
for i, (fwd, dz) in enumerate(((26.0, 17.0), (26.0, -17.0), (56.0, 17.0), (56.0, -17.0), (80.0, 17.0), (83.0, -18.0))):
    place("blk_full_siding_break%02d" % i, "full", (SID_ANCHOR - fwd, 0.0, SID_Z + dz), (3.0, 4.0, 3.0))

# ---- SHAPE: THE YARDS STOP BEING FLAT ---------------------------------------
# Owner: "theres absolutely no shape to fernhall at all just the main opened
# portion". He is right, and the reason is structural rather than decorative:
# every yard is one 106 x 56 m slab with cover standing on it, so however much
# cover goes down it is still a rectangle you cross.
#
# WHAT IS ADDED IS HEIGHT, NOT MORE COVER. Two raised decks per yard on
# opposite flanks, each with its own stair up and one of them running on into a
# catwalk over the flank. That gives a yard three things it did not have: a
# place to look down FROM, a place that is covered to walk UNDER, and a reason
# to leave the lane that is not just "there is a box over there".
#
# WHY NOT ALLEYS AND ROOMS, which is the other obvious answer: the cover
# grammar's dash-corridor floor requires 16 m of clear ground between any two
# full-height clusters, and an alley is 6 to 8. That floor is very probably a
# cause of the flatness he is describing, but it is a RULED number and
# relaxing it is the owner's call, not a thing to route around by picking a
# prefix the measurement does not look at. Recorded on the desk instead.
#
# EVERY DECK IS flr_ AND EVERY SUPPORT IS dress_. Floors carry collision, so a
# deck is walkable and a stair is climbable; dressing carries none, so the
# piers and rails cost the fight nothing and the space under a deck stays open
# to walk through. Nothing here is blk_*, so the cover grammar is untouched by
# construction rather than by luck.
#
# LATERALLY OUTBOARD OF EVERY POCKET, deliberately. The furthest a pocket sits
# off the lane is 15 m and its formation spreads 3 m either side, so 19 m is
# the first line where a structure cannot land on top of a fight. All O2
# PLACEHOLDER.
DECK_HEIGHT = 3.4
CATWALK_HEIGHT = 3.4
FAR_DECK_HEIGHT = 4.6
GANTRY_HEIGHT = 9.0
STEP_RISE = 0.85

def work_pass(tag, anchor_x, centre_z, bay_fwd, bay_side, dock_fwd, dock_side, forward=1.0):
    """A place where the yard's work happened: one enterable bay and one loading
    dock, per yard. forward is +1 for a yard that faces +x and -1 for one that
    faces -x; every piece here is a box scaled to a size, so the mirrored yard
    gets the same box at the mirrored place. The dock stair is the one piece
    with a run of its own, and it is placed by position alone for either sign
    exactly as it is for +x: which way its treads climb has not been
    photographed for either, and a yaw guessed here would be a fake.

    Owner: "the whole level itself doesn't feel good ... the goal is to have a
    decent starting area". What the yards had was cover, decks, gantries and a
    boundary — every one of them a thing to FIGHT around, and not one of them a
    thing that explains what the place was for. A yard you can walk into a
    building in is a different kind of space from a yard you can only cross.

    WHERE THIS IS LEGAL. The bay's walls are wall_ pieces, which bound space and
    are not measured as cover, and its floor and roof are flr_. That is the same
    vocabulary the perimeter and the gantry decks already use and it leaves
    every cover measurement untouched — the dash-corridor floor is not relaxed,
    and no piece of cover moved. What is NOT done here is building an alley out
    of blk_ pieces and hoping the measurement misses it.

    Both stand outboard of 18 m from the lane, clear of the gantry legs at 13 m
    and of the pocket positions down the middle."""
    def at(fwd, lat, height=0.0):
        return (anchor_x + forward * fwd, height, centre_z + lat)

    # --- THE BAY: fourteen metres by ten, six tall, one mouth ---------------
    # The mouth faces the lane, so it is somewhere you can be driven into or
    # break line of sight in, rather than a shed with its back to the fight.
    bw, bd, bh, wall = 14.0, 10.0, 6.0, 0.6
    cheek = (bw - 5.0) * 0.5           # a five-metre doorway, centred
    face = bay_side * (bd * 0.5)
    place("flr_%s_bay" % tag, "pavement", at(bay_fwd, bay_side * 21.0), (bw, 0.3, bd))
    place("flr_%s_bayroof" % tag, "pavement", at(bay_fwd, bay_side * 21.0, bh), (bw, 0.4, bd))
    for side, dx in enumerate((-(bw - cheek) * 0.5, (bw - cheek) * 0.5)):
        place("wall_%s_baycheek%d" % (tag, side), "panel",
              at(bay_fwd + dx, bay_side * 21.0 - face), (cheek, bh, wall))
    place("wall_%s_bayback" % tag, "panel", at(bay_fwd, bay_side * 21.0 + face), (bw, bh, wall))
    for side, dx in enumerate((-bw * 0.5, bw * 0.5)):
        place("wall_%s_bayside%d" % (tag, side), "panel",
              at(bay_fwd + dx, bay_side * 21.0), (wall, bh, bd))
    # What is inside it: stock, and a reason to look.
    for i, (dx, dz) in enumerate(((-4.4, 2.6), (-3.0, 2.6), (-3.7, 1.4), (4.6, -2.2), (4.6, 3.0))):
        place("dress_%s_baycrate%d" % (tag, i), "crate",
              at(bay_fwd + dx, bay_side * 21.0 + dz), (1.3, 1.3, 1.3))
    for i, dx in enumerate((-1.0, 1.6)):
        place("dress_%s_baybarrel%d" % (tag, i), "barrel",
              at(bay_fwd + dx, bay_side * 21.0 - face + bay_side * 1.6), (0.7, 1.2, 0.7))

    # --- THE DOCK: a metre and a half of ground, with stairs ---------------
    # Verticality you can stand on that is not nine metres up a gantry: high
    # enough to shoot down off, low enough to vault back from, and pressed
    # against the boundary so it reads as loading rather than as a platform
    # somebody put in a field.
    dh = 1.5
    place("flr_%s_dock" % tag, "pavement", at(dock_fwd, dock_side * 20.5, dh), (16.0, 0.4, 7.0))
    for side, dx in enumerate((-7.4, 7.4)):
        place("wall_%s_dockface%d" % (tag, side), "panel",
              at(dock_fwd + dx, dock_side * 20.5), (1.2, dh, 7.0))
    place("wall_%s_dockfront" % tag, "panel",
          at(dock_fwd, dock_side * 20.5 - dock_side * 3.5), (16.0, dh, 0.8))
    place("flr_%s_dockstair" % tag, "stairs",
          at(dock_fwd + 10.5, dock_side * 20.5), (4.0, dh, 5.0))
    for i, (dx, dz) in enumerate(((-5.0, 1.0), (-3.6, 1.0), (-4.3, -0.4), (5.2, 1.2))):
        place("dress_%s_dockcrate%d" % (tag, i), "crate",
              at(dock_fwd + dx, dock_side * 20.5 + dz, dh + 0.2), (1.3, 1.3, 1.3))
    place("dress_%s_dockcable" % tag, "cable",
          at(dock_fwd - 8.5, dock_side * 20.5, dh), (1.6, 0.2, 5.5))


def shape_pass(tag, anchor_x, centre_z, gantries, masses, forward=1.0):
    """One yard's verticality, authored once in the yard's own frame and
    instanced once per yard. fwd runs down the lane from the yard's anchor; lat
    is across it, positive toward +z. The entry yard, the substation and the
    depot face +x, so their frame is a translation; the siding faces -x, so
    forward = -1 and its frame is the translation mirrored. Every piece is a
    box scaled to a size, and a mirrored box is the same box."""
    def at(fwd, lat, height=0.0):
        return (anchor_x + forward * fwd, height, centre_z + lat)

    # --- The near deck, on the +lat flank -----------------------------------
    place("flr_%s_deck" % tag, "pavement", at(35.0, 23.0, DECK_HEIGHT), (18.0, 0.4, 8.0))
    # Piers, so the deck is a structure rather than a slab hanging in the air.
    for i, fwd in enumerate((27.5, 35.0, 42.5)):
        for j, lat in enumerate((19.8, 26.2)):
            place("dress_%s_pier%d%d" % (tag, i, j), "bldg_c", at(fwd, lat), (0.7, DECK_HEIGHT, 0.7))
    # The stair up: four flat treads rather than a ramp. A leaned slab would
    # have to survive place()'s bounding-box rescale, and a tread cannot be
    # walked up wrong.
    for i in range(4):
        place("flr_%s_step%d" % (tag, i), "pavement",
              at(23.0 + i * 2.6, 23.0, STEP_RISE * (i + 1)), (2.6, 0.35, 5.0))
    # A rail along the open edge, so the drop reads before it is stepped off.
    for i, fwd in enumerate((28.0, 34.0, 40.0)):
        place("dress_%s_rail%d" % (tag, i), "chest", at(fwd, 19.2, DECK_HEIGHT + 0.4), (5.6, 0.9, 0.2))

    # --- The catwalk, running on down the flank -----------------------------
    place("flr_%s_catwalk" % tag, "pavement", at(54.0, 23.0, CATWALK_HEIGHT), (20.0, 0.4, 4.0))
    for i, fwd in enumerate((48.0, 54.0, 60.0)):
        place("dress_%s_cwpier%d" % (tag, i), "bldg_c", at(fwd, 23.0), (0.6, CATWALK_HEIGHT, 0.6))
        place("dress_%s_cwrail%d" % (tag, i), "chest", at(fwd, 21.2, CATWALK_HEIGHT + 0.4), (5.6, 0.9, 0.2))

    # --- The far deck, higher and on the opposite flank ---------------------
    # Higher on purpose: two identical platforms are one platform twice, and
    # the yard should have a best position rather than a mirrored pair.
    place("flr_%s_fardeck" % tag, "pavement", at(62.0, -23.0, FAR_DECK_HEIGHT), (16.0, 0.4, 8.0))
    for i, fwd in enumerate((55.5, 62.0, 68.5)):
        for j, lat in enumerate((-19.8, -26.2)):
            place("dress_%s_fpier%d%d" % (tag, i, j), "bldg_c", at(fwd, lat), (0.7, FAR_DECK_HEIGHT, 0.7))
    for i in range(5):
        place("flr_%s_fstep%d" % (tag, i), "pavement",
              at(50.0 + i * 2.6, -23.0, STEP_RISE * (i + 1)), (2.6, 0.35, 5.0))
    for i, fwd in enumerate((56.0, 62.0, 68.0)):
        place("dress_%s_frail%d" % (tag, i), "chest", at(fwd, -19.2, FAR_DECK_HEIGHT + 0.4), (5.6, 0.9, 0.2))

    # --- GANTRIES OVER THE LANE ---------------------------------------------
    # "theres absolutely no shape to fernhall at all" was said AGAIN after the
    # decks landed, and he was right: they hug the perimeter at 19 to 27 m out,
    # so the thing the player actually walks down — 106 m of clear floor with
    # nothing above 4 m in it — was untouched. A yard you can see the whole
    # length of is a rectangle however much you build along its edges.
    #
    # So this crosses it. A span at nine metres with its legs outboard of the
    # dash corridor: it cuts the long sightline, it puts something OVER the
    # player, and it makes the lane a sequence of spaces instead of one run.
    #
    # NOT WALKABLE, deliberately. Reaching nine metres needs eleven treads and
    # twenty-eight metres of run, which would be a bigger structure than the
    # yard has room for; the decks are where height is walked. This is a pipe
    # bridge over an industrial yard, which is what it should look like.
    #
    # The legs stand at 13 m off the lane — outboard of the 9 m dash corridor
    # the cover grammar protects, and inboard of the decks — so the player
    # walks between them.
    for i, fwd in enumerate(gantries):
        place("flr_%s_gantry%d" % (tag, i), "pavement", at(fwd, 0.0, GANTRY_HEIGHT), (5.0, 0.5, 30.0))
        for j, lat in enumerate((-13.0, 13.0)):
            place("wall_%s_gleg%d%d" % (tag, i, j), "bldg_c", at(fwd, lat), (2.0, GANTRY_HEIGHT, 2.0))
            place("dress_%s_gbrace%d%d" % (tag, i, j), "chest",
                  at(fwd, lat * 0.72, GANTRY_HEIGHT - 1.6), (1.0, 1.6, 8.0))
        # A rail down each edge of the span, so it reads as a walkway from
        # below rather than as a slab floating over the yard.
        for j, lat in enumerate((-14.0, 14.0)):
            place("dress_%s_grail%d%d" % (tag, i, j), "chest",
                  at(fwd, lat, GANTRY_HEIGHT + 0.6), (4.0, 1.2, 0.25))

    # --- ONE BIG MASS, OFF THE LANE -----------------------------------------
    # Twelve metres tall and eleven across: not cover, a BUILDING. Cover tops
    # out at four metres, so no amount of it breaks a sightline down a yard —
    # this does, and it makes the far half of the yard something you come
    # around rather than something you can see from the door.
    #
    # wall_ rather than blk_full_, and that is not prefix-shopping: the
    # perimeter buildings are wall_ pieces already, so this is the same
    # vocabulary at the same scale, standing inside instead of around. As
    # blk_full_ it would enter the cover footprint band (0.5-5% of the yard)
    # and one of these is 2% on its own.
    for i, (fwd, lat) in enumerate(masses):
        place("wall_%s_mass%d" % (tag, i), "bldg_a", at(fwd, lat), (11.0, 12.0, 9.0))
        # A lower shoulder against it, so the mass has a silhouette rather than
        # being one box.
        place("wall_%s_massb%d" % (tag, i), "garage",
              at(fwd + 7.0, lat + (2.5 if lat > 0 else -2.5)), (6.0, 6.5, 7.0))

# WHERE THE STRUCTURE GOES, PER YARD, and the positions are not free choices.
# A pocket sits every 14 + 75*fraction metres down the lane and its arrival tear
# sits 7.5 m behind it; a leg or a mass landing on either would spawn a fight
# inside a building. These are the gaps left between them in each yard, which is
# why the lists differ instead of being one shared set.
#
#   entry  pockets  32.75  47.75  66.5   (+ tears at 40.25  55.25  74.0)
#   sub    pockets  51.5   74.0          (+ tears at 59.0   81.5)
#   depot  pockets  32.75  55.25  77.75  (+ tears at 40.25  62.75  85.25)
#   siding pockets  32.75  47.75  66.5   (+ tears at 40.25  55.25  74.0)
#
# The siding opens on the ENTRY yard's pocket pattern (BreakerGameMode's
# outdoor pocket table: 0.25 and 0.70 on the lane, 0.45 off it), so it takes
# the entry yard's structure list and not the substation's — the substation's
# gantry at 66 would stand over the siding's pocket at 66.5.
shape_pass("entry", 6.0, 0.0, gantries=(20.0, 82.0), masses=((60.0, -19.0),))
# The bay and the dock go in the gaps the pockets and their tears leave, on the
# flank the yard's building mass is NOT on, so one side of each yard is a place
# to work and the other is a place to climb.
work_pass("entry", 6.0, 0.0, bay_fwd=44.0, bay_side=1.0, dock_fwd=88.0, dock_side=-1.0)
shape_pass("sub", SUB_ANCHOR, SUB_Z, gantries=(20.0, 66.0), masses=((40.0, 19.0),))
work_pass("sub", SUB_ANCHOR, SUB_Z, bay_fwd=34.0, bay_side=-1.0, dock_fwd=88.0, dock_side=1.0)
shape_pass("dep", DEP_ANCHOR, DEP_Z, gantries=(20.0, 70.0), masses=((48.0, -19.0),))
work_pass("dep", DEP_ANCHOR, DEP_Z, bay_fwd=15.0, bay_side=1.0, dock_fwd=68.0, dock_side=-1.0)
# The dock at fwd 88 puts its stair at fwd 98.5..102.5, past the siding slab's
# far edge at fwd 94 and outside its end wall — as the same dock already does
# in the entry yard (stair x 102.5..106.5, slab to 104) and the substation
# (x 166.5..170.5, slab to 164). Copied, not corrected: the list is the entry
# yard's, and moving the dock is its own item.
shape_pass("siding", SID_ANCHOR, SID_Z, gantries=(20.0, 82.0), masses=((60.0, -19.0),), forward=-1.0)
work_pass("siding", SID_ANCHOR, SID_Z, bay_fwd=44.0, bay_side=1.0, dock_fwd=88.0, dock_side=-1.0, forward=-1.0)

# ---- Markers ----------------------------------------------------------------
# THE NAME CARRIES A ROLE AND A YARD, and this is the authoring side of a
# contract with two readers — BreakerZoneBuilder::ParseMarkerName and
# breaker_import_fernhall.py's parse_marker. Change it here and both refuse
# the export rather than importing a zone that is quietly missing something.
#
#   marker_<role>          this marker belongs to the ENTRY yard
#   marker_<role>_<yard>   it belongs to <yard>
#
# Roles: playerstart, rift, npc_contract, yard, spawn. An unknown role is
# REFUSED, not skipped, so a typo here is a loud failure rather than a marker
# that silently does not exist.
#
# EXACTLY ONE playerstart per zone. Rift doors and contract givers are
# per-yard and OPTIONAL — a yard with no door is a legal yard — but no
# (role, yard) pair may repeat: two rift markers in one yard would spawn two
# doors on the same spot.
#
# THE spawn ROLE CARRIES AN INDEX, because a yard has several of them (O274):
#
#   marker_spawn_<n>          the ENTRY yard's n-th patrol return point
#   marker_spawn_<yard>_<n>   yard <yard>'s n-th
#
# <n> is 0-based and the index is what keeps the no-repeat rule its shape: the
# pair that may not repeat is (spawn, <yard>, <n>). A spawn marker is where a
# patrol returns FROM — a bay mouth, a dock face, a seam mouth — authored
# ground first, so a tear is used only where the composer authored nothing.
# The yard tag is the one that yard's other markers use (substation, depot,
# siding; the entry yard carries no tag), and the entry yard's index form
# has no yard segment for the same reason its other markers have none.
#
# EVERY NAMED YARD NEEDS A `yard` ANCHOR. A yard's grammar is measured in its
# OWN frame, and a zone has exactly one playerstart, so the rule that anchors
# the entry yard cannot anchor a second: marker_yard_<name> is what gives yard
# <name> a frame. The ENTRY yard is exempt because the playerstart anchors it.
#
# The three below carry no yard suffix because they are the entry yard's.
# Growing the zone means adding marker_yard_<name> plus that yard's own
# markers, not changing any of this — the substation, the depot and the siding
# below are each exactly that.
place("marker_playerstart", None, (6.0, 0.0, 0.0), marker=True)
place("marker_rift", None, (92.0, 0.0, 0.0), marker=True)
place("marker_npc_contract", None, (13.0, 0.0, -14.0), marker=True)

# The SUBSTATION yard's anchor and its own rift door. The anchor is what gives
# this yard a frame: a zone has exactly one playerstart, so nothing else could.
# Its forward is derived the same way the entry yard's is — from what it points
# at, which is its own rift.
place("marker_yard_substation", None, (SUB_X - 41.0, 0.0, SUB_Z), marker=True)
place("marker_rift_substation", None, (SUB_X + 41.0, 0.0, SUB_Z), marker=True)

# The DEPOT yard's anchor, and nothing else: no rift marker, because it has no
# door. YardFrame falls back to +X when a yard points at no rift, which is the
# direction this yard runs anyway.
place("marker_yard_depot", None, (DEP_ANCHOR, 0.0, DEP_Z), marker=True)

# The SIDING's anchor and its own rift door (O276). No playerstart: a zone has
# exactly one, and it is the entry yard's. The anchor is the yard's east end
# and the rift its west, so the forward YardFrame derives from the pair is -X;
# nothing else about the marker contract changes for a yard that faces the
# other way.
place("marker_yard_siding", None, (SID_ANCHOR, 0.0, SID_Z), marker=True)
place("marker_rift_siding", None, (SID_X - 41.0, 0.0, SID_Z), marker=True)

# ---- Patrol return points (O274) -------------------------------------------
# Where a patrol comes back from is authored ground first: the bay's doorway,
# the dock's face, and every seam mouth on the yard's flank. Each is pushed
# 1.5 m from the opening into the yard — the walk-out point, not the threshold
# — so an arrival stands on the yard slab and not in a wall. Floor height;
# facing is NOT authored, the game faces the arrival toward its post.
#
# The bay and dock points are read off work_pass's own numbers: the doorway
# is the cheek line at |lat| 21 - bd/2 = 16, the dock face is the dockfront at
# |lat| 20.5 - 3.5 = 17. Seam mouths are absolute glTF (x, z), each 1.5 m
# inside the perimeter's INNER face (the wall line +- 1.5) on the mouth's
# centre line.
SPAWN_STANDOFF = 1.5   # O2 PLACEHOLDER

def spawn_pass(yard, anchor_x, centre_z, bay_fwd, bay_side, dock_fwd, dock_side, mouths, forward=1.0):
    """Index 0 is the bay walk-out, 1 the dock face, 2.. the seam mouths in
    the order given. yard is the marker tag ('' for the entry yard), matching
    the yard's other markers."""
    def at(fwd, lat):
        return (anchor_x + forward * fwd, 0.0, centre_z + lat)
    def name(n):
        return "marker_spawn_%s%d" % (yard + "_" if yard else "", n)
    place(name(0), None, at(bay_fwd, bay_side * (16.0 - SPAWN_STANDOFF)), marker=True)
    place(name(1), None, at(dock_fwd, dock_side * (17.0 - SPAWN_STANDOFF)), marker=True)
    for i, (x, z) in enumerate(mouths):
        place(name(2 + i), None, (x, 0.0, z), marker=True)

# Entry: the east mouth to the substation (z 9..19, wall_e inner face x 100)
# and the plaza's south mouth to the siding (x 0..10, inner face z -23.5).
spawn_pass("", 6.0, 0.0, bay_fwd=44.0, bay_side=1.0, dock_fwd=88.0, dock_side=-1.0,
           mouths=((100.0 - SPAWN_STANDOFF, 14.0), (5.0, -23.5 + SPAWN_STANDOFF)))
# Substation: the south mouth where the first seam arrives (x 106..116, inner
# face z 37.5) and the north mouth the second leaves by (x 130..140, inner
# face z 84.5).
spawn_pass("substation", SUB_ANCHOR, SUB_Z, bay_fwd=34.0, bay_side=-1.0, dock_fwd=88.0, dock_side=1.0,
           mouths=((SUB_X, SUB_Z - 23.5 + SPAWN_STANDOFF), (135.0, SUB_Z + 23.5 - SPAWN_STANDOFF)))
# Depot: one mouth, in the west end wall (z 95..105, inner face x 154.5).
spawn_pass("depot", DEP_ANCHOR, DEP_Z, bay_fwd=15.0, bay_side=1.0, dock_fwd=68.0, dock_side=-1.0,
           mouths=((DEP_X - 52.0 + SPAWN_STANDOFF, 100.0),))
# Siding: one mouth, in the east end wall (z -53..-43, inner face x -5).
spawn_pass("siding", SID_ANCHOR, SID_Z, bay_fwd=44.0, bay_side=1.0, dock_fwd=88.0, dock_side=-1.0,
           mouths=((SID_X + 52.0 - SPAWN_STANDOFF, -48.0),), forward=-1.0)

# ---- Dressing (O24: vegetation over ruins) ---------------------------------
# WHERE PLANTS GROW. Owner: "some graphical assets that are imported that are
# placed randomly, like trees". They were: six clumps per yard at coordinates
# chosen by hand to be spread out, which is the one arrangement nature never
# produces in a paved yard. Vegetation reclaims ground from the edges — it
# comes through where the slab meets a wall, fills the corners nothing drives
# through, and stands where a building's shadow kept the concrete damp. So
# every clump stands against a boundary or in a corner, with grass at its
# foot, and the open lane stays open because that is where the traffic was.
def grow(tag, anchor_x, centre_z, corners=((2.5, 23.0), (2.5, -23.0)), forward=1.0):
    """Clumps against the flanks and in the corners of one yard, frame-
    relative like everything else that was authored from the validated first
    yard rather than by eye. forward is the yard's sign along x, as in
    shape_pass."""
    def at(fwd, lat, height=0.0):
        return (anchor_x + forward * fwd, height, centre_z + lat)
    # Against the north and south flanks, pressed to the boundary face at
    # 23.5 m, at forward positions that avoid every dock, bay, gantry leg and
    # deck pier the yard has.
    for i, (fwd, lat) in enumerate(((9.0, 22.5), (57.0, -22.6), (73.0, 22.4), (26.0, -22.3))):
        place("dress_%s_tree%d" % (tag, i), "trees", at(fwd, lat), (5.0, 4.2, 3.4))
        place("dress_%s_treegrass%d" % (tag, i), "grass", at(fwd + 1.5, lat - (1.8 if lat > 0 else -1.8)))
    # The corners: one bigger clump in each of the two the entrance does not
    # use, with a grass mound under it where the slab has heaved. A yard whose
    # mouth is at a corner passes that corner's clump a place further down the
    # same flank, so the clump stands beside the door and not in it.
    for i, (fwd, lat) in enumerate(corners):
        place("dress_%s_corner%d" % (tag, i), "trees", at(fwd, lat), (6.5, 5.0, 4.5))
        place("dress_%s_cornermound%d" % (tag, i), "mound", at(fwd + 1.0, lat), (7.0, 0.5, 5.0))
    # Grass in the seams of the slab: along the boundary foot, never in the
    # lane.
    for i, fwd in enumerate((18.0, 41.0, 66.0, 90.0)):
        place("dress_%s_seamgrass%d" % (tag, i), "grass", at(fwd, 22.0 if i % 2 else -22.0))

# The entry yard's south-west corner is the third seam's mouth (x 0..10), so
# its clump moves from x 8.5 to x 20, against the same flank; the siding's
# mouth is its north-east corner, and its clump moves the same 11.5 m down its
# own flank.
grow("entry", 6.0, 0.0, corners=((2.5, 23.0), (14.0, -23.0)))
grow("sub", SUB_ANCHOR, SUB_Z)
grow("dep", DEP_ANCHOR, DEP_Z)
grow("siding", SID_ANCHOR, SID_Z, corners=((14.0, 23.0), (2.5, -23.0)), forward=-1.0)

# ---- THE RUIN, dressing only (--ruined) ------------------------------------
# Placed against the cover the yard already has rather than instead of it, so
# every measured box is identical in both targets and the fight is the same
# fight. Deterministic: the offsets and angles are a fixed table walked in
# order, so the ruined yard is the SAME ruined yard every time it is composed —
# a rift that rearranged itself between builds could not be photographed or
# bug-reported.
if RUINED:
    RUIN_PLAN = ((0.0, 25.0), (1.0, 200.0), (-1.0, 110.0), (0.6, 295.0), (-0.7, 65.0), (1.3, 155.0))
    Ruins = 0

    def ruin_beside(anchor_name, dx, dz, key, size, yaw, lean):
        """One chunk leaned against a piece the yard already placed. Reads the
        anchor's baked bounds so the rubble follows the cover it belongs to
        rather than a second copy of the cover's coordinates."""
        global Ruins
        if anchor_name not in SCENE:
            return
        b = SCENE[anchor_name].bounds
        centre = (b[0] + b[1]) * 0.5
        place("dress_ruin%02d" % Ruins, key, (centre[0] + dx, 0.0, centre[2] + dz),
              size, yaw=yaw, lean=lean)
        Ruins += 1

    # COLLAPSE, NOT CLUTTER. Owner: "the dilapidated just looks like a mess.
    # It doesn't even look dilapidated, it just has random assets that are
    # broken laying in random places." It did: one chunk leaned against each
    # piece of cover, evenly, down the whole yard — rubble with no building it
    # came from. A ruin is a building that FELL, and what fell lands in a fan
    # below the place it left: big pieces near the wall, smaller further out,
    # a slab still leaning on what is left standing, and the vegetation
    # thickest right there, because that is the ground nothing has cleared.
    def collapse(tag, wall_name, into, seed):
        """A fan of debris at the foot of one perimeter building, spreading
        `into` the yard (+1 or -1 in z). Reads the building's baked bounds."""
        global Ruins
        if wall_name not in SCENE:
            return
        b = SCENE[wall_name].bounds
        cx = (b[0][0] + b[1][0]) * 0.5
        face = b[0][2] if into < 0 else b[1][2]     # the face toward the yard
        # The slab that came off the face, leaning back on it.
        lean, yaw = RUIN_PLAN[seed % len(RUIN_PLAN)]
        place("dress_ruin%02d" % Ruins, "ruin_slab", (cx - 1.5, 0.0, face + into * 1.6),
              (1.6, 4.2, 3.6), yaw=180.0 if into < 0 else 0.0, lean=-into * 22.0)
        Ruins += 1
        # The fan: six chunks, biggest nearest the wall, each further out and
        # a little further along than the last, each at its own angle.
        for i in range(6):
            lean, yaw = RUIN_PLAN[(seed + i) % len(RUIN_PLAN)]
            out = 1.2 + 1.1 * i
            along = (i % 3 - 1) * 1.7 + (0.6 if i % 2 else -0.6)
            size = max(0.5, 1.0 - 0.13 * i)
            place("dress_ruin%02d" % Ruins, "ruin_chunk", (cx + along, 0.0, face + into * out),
                  (size * 1.1, size * 1.4, size * 2.2), yaw=yaw + 30.0 * i, lean=55.0 + lean * 12.0)
            Ruins += 1
        # And the ground has taken it back: grass through the rubble and one
        # clump against the standing stub.
        place("dress_ruin%02d" % Ruins, "grass", (cx + 1.8, 0.0, face + into * 3.0))
        Ruins += 1
        place("dress_ruin%02d" % Ruins, "trees", (cx - 3.2, 0.0, face + into * 1.2), (4.0, 3.4, 2.8))
        Ruins += 1

    # Two collapses per yard, on different flanks, at buildings that do not
    # carry a canopy, a bay or a dock — those are the profiles that stayed up.
    for tag, (n, s_) in (("", ("wall_n03", "wall_s06")),
                         ("sub", ("wall_sub_n02", "wall_sub_s07")),
                         ("dep", ("wall_dep_n05", "wall_dep_s01")),
                         ("siding", ("wall_siding_n09", "wall_siding_s10"))):
        collapse(tag, n, -1, 1)
        collapse(tag, s_, 1, 4)

    # A few chunks still beside the full-height breaks in the lane, standing
    # close to upright: the pieces that carry the skyline at distance. Fewer
    # than before, and only at the breaks, so the lane reads as a place things
    # fell INTO from the sides rather than as a field of props.
    #
    # Walked yard by yard in the order the first three sorted to — entry, depot,
    # substation — with the siding after them, so a fourth yard's breaks take
    # new rows of the plan instead of shifting which side of a substation break
    # its chunk leans on.
    BREAK_ORDER = ("blk_full_break", "blk_full_dep_break", "blk_full_sub_break", "blk_full_siding_break")
    Full = [n for prefix in BREAK_ORDER for n in sorted(SCENE) if n.startswith(prefix)]
    for i, name in enumerate(Full):
        if i % 2:
            continue
        lean, yaw = RUIN_PLAN[i % len(RUIN_PLAN)]
        ruin_beside(name, -2.6 if i % 4 else 2.6, 1.1 * lean, "ruin_slab", (1.4, 3.4, 3.0), yaw, 14.0 + lean * 6.0)

    print("ruin dressing:", Ruins, "chunks")

# THE ROSTER THIS WRITES: 645 meshes intact, 729 ruined — the 84 ruin chunks
# are dressing over the same 645. Four yards (entry 57 with its south mouth
# open, substation 56, depot 60, siding 64), three seams (6, 7, 5), four times
# the 92 a shape, work and grow pass add, and 22 markers (8 frame and door
# markers plus 14 spawn points: entry 4, substation 4, depot 3, siding 3).
# BreakerFernhallExpectedPieceCount (BreakerFernhallZoneTests.cpp) and
# EXPECTED_TOTAL (breaker_import_fernhall.py) are kept by hand to these.
scene = trimesh.Scene(SCENE)
os.makedirs(os.path.dirname(OUT), exist_ok=True)
scene.export(OUT)
print("wrote", OUT, "meshes:", len(SCENE), "ruined" if RUINED else "intact")
