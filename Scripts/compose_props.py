# The props the runtime spawns on their own — AUTHORED HERE (O280).
#
#   python Scripts/compose_props.py
#
# Reads the megakit pieces in Assets/zones/kit through the same loader the
# yard composer uses (breaker_kit.py) and writes Assets/zones/props.glb: one
# scene, three named meshes, each sized to the player and grounded at its own
# origin, so a C++ hand spawns the static mesh at an actor's transform and
# the prop stands on the floor where the actor is. Content/Python/
# breaker_import_props.py splits the scene into per-name static meshes under
# /Game/Breaker/Meshes/props.
#
#   prop_chest_body   the supply chest's crate: Prop_Chest without its lid,
#                     grounded (min-Y 0) and centred on X/Z
#   prop_chest_lid    the lid, in the SAME scale and the same frame as the
#                     body, then moved so its ORIGIN IS THE HINGE — the lid's
#                     back-bottom edge. A rotation of the lid component about
#                     its local X axis opens it; the hinge point in the body's
#                     frame is printed below and is what the hand offsets the
#                     lid component by.
#   prop_crate4       the kit's 1.121 m crate, native size, grounded
#
# Authored in glTF space: Y up. The chest is left at its NATIVE facing (the
# lid's free edge, its lip, is glTF +Z; the hinge runs along glTF X at glTF
# -Z) and the export log says so; the importer's axis mapping is not guessed
# here, the C++ hand rotates the component.
import os
import sys
import numpy as np
import trimesh

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from breaker_kit import KIT, KitResolver, _pack_parts, load_piece, concat_shared

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
OUT = os.path.join(ROOT, "Assets", "zones", "props.glb")

CHEST = "modular-sci-fi-megakit/glTF/Props/Prop_Chest.gltf"
CRATE = "modular-sci-fi-megakit/glTF/Props/Prop_Crate4.gltf"

# THE CHEST'S PARTS, BY NAME. The kit ships the chest as two skinned meshes
# under two nodes, Prop_Chest (the body) and Prop_Chest_Top (the lid), each
# with two primitives on two trim materials; trimesh names a primitive's node
# after the node it hangs from, so the lid is every part whose node — or any
# node above it — starts with LID_NODE. The rest is the body.
LID_NODE = "Prop_Chest_Top"
# The hinge: the Chest_Top bone's rest translation, the lid's back-bottom
# edge, in the kit's native metres. Read off the glTF, not measured.
HINGE_NATIVE = np.array([0.0, 0.4155474603176117, -0.36449527740478516])
# The lid spans native Y 0.415..0.732; the split is checked against that so
# a renamed node fails loudly here rather than exporting a lidless chest.
LID_MIN_Y_NATIVE = 0.4147
CHEST_TOP_NATIVE = 0.7322

# The body is 1.10 m wide (O2 PLACEHOLDER; native 1.503), and the lid takes
# the same factor: a chest is a thing the player crouches at.
CHEST_WIDTH = 1.10   # O2 PLACEHOLDER


def _named_parts(name):
    """breaker_kit._kit_parts with the names kept: the same load, the same
    resolver, the same per-node bake in the same order, and beside each part
    its node, its geometry and every node above it. _kit_parts drops the
    names, and the lid split needs them."""
    path = os.path.join(KIT, name)
    scene = trimesh.load(path, force="scene", resolver=KitResolver(os.path.dirname(path)))
    try:
        parents = scene.graph.transforms.parents
    except AttributeError:
        parents = {}
    named = []
    for node in scene.graph.nodes_geometry:
        transform, geometry = scene.graph[node]
        part = scene.geometry[geometry].copy()
        part.apply_transform(transform)
        lineage = [str(node), str(geometry)]
        up = node
        while up in parents:
            up = parents[up]
            lineage.append(str(up))
        named.append((lineage, part))
    return named


def compose_chest():
    named = _named_parts(CHEST)
    lid_parts = [part for lineage, part in named if any(n.startswith(LID_NODE) for n in lineage)]
    body_parts = [part for lineage, part in named if not any(n.startswith(LID_NODE) for n in lineage)]
    assert lid_parts and body_parts, ([l for l, _ in named])
    # ONE PACK FOR EVERY PART: body and lid share one atlas and one material,
    # so the GLB carries the chest's sheets once and the two meshes match.
    material = _pack_parts(lid_parts + body_parts)
    lid = concat_shared(lid_parts, material)
    body = concat_shared(body_parts, material)
    lid_b, body_b = lid.bounds, body.bounds
    assert abs(lid_b[0][1] - LID_MIN_Y_NATIVE) < 0.01, lid_b
    assert abs(lid_b[1][1] - CHEST_TOP_NATIVE) < 0.01, lid_b
    assert body_b[0][1] < 0.01, body_b
    native_width = body_b[1][0] - body_b[0][0]

    # Uniform scale, body and lid alike, then ground the body and move the
    # lid by the same translation so the two stay one chest.
    scale = CHEST_WIDTH / native_width
    body.apply_scale(scale)
    lid.apply_scale(scale)
    b = body.bounds
    ground = np.array([-(b[0][0] + b[1][0]) * 0.5, -b[0][1], -(b[0][2] + b[1][2]) * 0.5])
    body.apply_translation(ground)
    lid.apply_translation(ground)
    hinge = HINGE_NATIVE * scale + ground   # the hinge in the body's frame
    # The lid's origin is its hinge: rotate the component about local X to open.
    lid.apply_translation(-hinge)

    print("chest: native width %.3f m, scale %.4f, body %.3f wide" % (native_width, scale, CHEST_WIDTH))
    print("chest: lid hinge in body frame (glTF x, y, z) = (%.4f, %.4f, %.4f) m" % tuple(hinge))
    print("chest: NATIVE FACING kept — the lid's free edge (lip) is glTF +Z, the hinge line runs along glTF X at glTF -Z;"
          " the lid opens by rotating about its local X axis")
    return body, lid


def compose_crate():
    crate = load_piece(CRATE)
    b = crate.bounds
    crate.apply_translation((-(b[0][0] + b[1][0]) * 0.5, -b[0][1], -(b[0][2] + b[1][2]) * 0.5))
    return crate


def report(name, mesh):
    b = mesh.bounds
    size = b[1] - b[0]
    print("%-16s min (%7.3f, %7.3f, %7.3f)  max (%7.3f, %7.3f, %7.3f)  size (%.3f x %.3f x %.3f) m  %d faces"
          % (name, b[0][0], b[0][1], b[0][2], b[1][0], b[1][1], b[1][2], size[0], size[1], size[2], len(mesh.faces)))


body, lid = compose_chest()
SCENE = {
    "prop_chest_body": body,
    "prop_chest_lid": lid,
    "prop_crate4": compose_crate(),
}
for name in SCENE:
    report(name, SCENE[name])

scene = trimesh.Scene(SCENE)
os.makedirs(os.path.dirname(OUT), exist_ok=True)
scene.export(OUT)
print("wrote", OUT, "meshes:", len(SCENE))
