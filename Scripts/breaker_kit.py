# The kit loader every composer shares: how a CC0 kit piece in Assets/zones/kit
# becomes a trimesh with one packed atlas and a rough painted-metal surface.
#
#   from breaker_kit import load_piece, load_group, concat_shared
#
# compose_fernhall.py (the zone) and compose_props.py (the props the runtime
# spawns on their own) load the same kit through the same resolver, the same
# atlas caps and the same sheet lift, so a crate in the yard and the crate the
# supply chest is built from wear one surface. Lifted from compose_fernhall.py
# unchanged: the composer's geometry is byte-identical across the move.
import os
import numpy as np
import trimesh
from PIL import Image

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
KIT = os.path.join(ROOT, "Assets", "zones", "kit")

# THE KIT'S TEXTURES LIVE TWO FOLDERS UP. A megakit glTF names its images by
# bare file name and the images sit in <kit>/Textures, so a plain load finds
# nothing, packs five empty materials into a 32 x 8 black atlas, and every
# kit piece imports black the moment the builder stops painting over it
# (O279). The resolver looks beside the glTF first, then in Textures.
MEGAKIT_TEXTURES = os.path.join(KIT, "modular-sci-fi-megakit", "Textures")

class KitResolver(trimesh.resolvers.FilePathResolver):
    def get(self, name):
        try:
            return super().get(name)
        except Exception:
            with open(os.path.join(MEGAKIT_TEXTURES, os.path.basename(str(name))), "rb") as handle:
                return handle.read()

# One atlas per piece KIND, capped: the kit's sheets are 2048 squared and
# five of them fused is a 16k x 4k image per piece. 512 a sheet, 1024 fused,
# is a placeholder resolution at 4 m tiles. O2 PLACEHOLDER.
ATLAS_SHEET_PX = 512
ATLAS_FUSED_PX = 2048

def _kit_parts(name):
    path = os.path.join(KIT, name)
    scene = trimesh.load(path, force="scene", resolver=KitResolver(os.path.dirname(path)))
    parts = []
    for node in scene.graph.nodes_geometry:
        transform, geometry = scene.graph[node]
        part = scene.geometry[geometry].copy()
        part.apply_transform(transform)
        parts.append(part)
    return parts

# THE KIT IS PAINTED METAL, NOT MIRROR. Its ORM sheets mark every trim panel
# fully metallic, and a metal that reflects an empty Lumen scene reads black
# under one sun and a sky. The sheets are dropped and the surface is a rough
# dielectric, so the base colour is what shows. O2 PLACEHOLDER, judged by
# photograph; the kit's normal maps stay.
KIT_ROUGHNESS = 0.85
# The trim sheets average 46% grey and read as soot beside the concrete the
# builder paints; lifted so a kit wall in shadow still separates from the
# ground. O2 PLACEHOLDER.
KIT_SHEET_LIFT = 1.6
_LIFTED = {}

def _lift_sheet(image):
    key = id(image)
    if key not in _LIFTED:
        rgba = np.asarray(image.convert("RGBA")).astype(np.float32)
        rgba[..., :3] = np.clip(rgba[..., :3] * KIT_SHEET_LIFT, 0, 255)
        _LIFTED[key] = Image.fromarray(rgba.astype(np.uint8), "RGBA")
    return _LIFTED[key]

def _pack_parts(parts):
    for part in parts:
        material = part.visual.material
        material.metallicRoughnessTexture = None
        material.metallicFactor = 0.0
        material.roughnessFactor = KIT_ROUGHNESS
        if material.baseColorTexture is not None:
            material.baseColorTexture = _lift_sheet(material.baseColorTexture)
    material, uvs = trimesh.visual.material.pack(
        [part.visual.material for part in parts],
        [part.visual.uv if part.visual.uv is not None else np.zeros((len(part.vertices), 2)) + 0.5 for part in parts],
        max_tex_size_individual=ATLAS_SHEET_PX, max_tex_size_fused=ATLAS_FUSED_PX)
    # pack() hands back ONE stacked UV array over every vertex it was given;
    # split it back by vertex count.
    uvs = np.asarray(uvs)
    offset = 0
    for part in parts:
        part.visual = trimesh.visual.TextureVisuals(uv=uvs[offset:offset + len(part.vertices)], material=material)
        offset += len(part.vertices)
    assert offset == len(uvs), (offset, len(uvs))
    return material

def load_piece(name):
    path = os.path.join(KIT, name)
    if not name.endswith(".gltf"):
        return trimesh.load(path, force="mesh")
    parts = _kit_parts(name)
    return concat_shared(parts, _pack_parts(parts))

def load_group(names):
    """Several kit pieces packed into ONE atlas, so copies of any of them
    concatenate without a second pack: the facade tiles share their trim
    sheets, and the pack deduplicates them."""
    groups = [_kit_parts(name) for name in names]
    material = _pack_parts([part for parts in groups for part in parts])
    return [concat_shared(parts, material) for parts in groups]

def concat_shared(meshes, material):
    """Concatenate meshes that already share one material, without trimesh's
    concatenate: that call re-packs an atlas per result, and two hundred
    facades would carry two hundred atlases. Here the UVs stack and the one
    material is kept, so the GLB holds it once."""
    vertices = np.vstack([mesh.vertices for mesh in meshes])
    faces, offset, uvs = [], 0, []
    for mesh in meshes:
        faces.append(mesh.faces + offset)
        offset += len(mesh.vertices)
        uvs.append(mesh.visual.uv)
    return trimesh.Trimesh(vertices=vertices, faces=np.vstack(faces),
                           visual=trimesh.visual.TextureVisuals(uv=np.vstack(uvs), material=material),
                           process=False)
