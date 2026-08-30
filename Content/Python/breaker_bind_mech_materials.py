"""Binds the mech cast's vendored textures to their imported skeletal meshes.

The Animated Mech Pack's FBX embed NO texture references (checked in the
binaries: zero path strings), so the import produced flat-shaded hulls and no
importer setting can fix that — the wiring the FBX never carried is authored
here instead: import each mech's base texture, build a one-node material
(texture -> BaseColor, fully rough), and assign it to every material slot on
that mech's skeletal mesh. Idempotent: re-running reimports the texture,
rebuilds the same connection and reassigns.

The 16 recolor variations are vendored beside the bases and deliberately NOT
consumed yet — per-rank recolors are a presentation ruling (rank paint
currently speaks the hidden primitives' language), not an import.

Run (headless, editor closed):
  UnrealEditor-Cmd.exe <project> -run=pythonscript -script="breaker_bind_mech_materials.py"
"""

import os
import unreal

PROJECT_DIR = unreal.SystemLibrary.get_project_directory()
SRC = os.path.normpath(os.path.join(PROJECT_DIR, "Assets", "enemies", "animated-mech", "Textures"))
MECHS = ("George", "Leela", "Mike", "Stan")
DEST_ROOT = "/Game/Breaker/Meshes/enemies/mechs"

tools = unreal.AssetToolsHelpers.get_asset_tools()
matlib = unreal.MaterialEditingLibrary

for mech in MECHS:
    dest = "%s/%s" % (DEST_ROOT, mech)
    tex_file = os.path.join(SRC, "%s_Texture.png" % mech)
    if not os.path.isfile(tex_file):
        raise RuntimeError("[MechMat] missing texture: %s" % tex_file)

    task = unreal.AssetImportTask()
    task.filename = tex_file
    task.destination_path = dest
    # T_ prefix, NOT the source stem: the FBX import already made a
    # MaterialInstanceConstant named <Mech>_Texture (the pack names materials
    # after their textures), and importing a Texture2D onto that name was
    # refused as "nothing to import" — the collision, not the file.
    task.destination_name = "T_%s" % mech
    task.automated = True
    task.replace_existing = True
    task.save = True
    tools.import_asset_tasks([task])
    texture = unreal.load_asset("%s/T_%s" % (dest, mech))
    if not texture:
        raise RuntimeError("[MechMat] texture import produced nothing for %s" % mech)

    mat_path = "%s/M_%s" % (dest, mech)
    material = unreal.load_asset(mat_path)
    if not material:
        material = tools.create_asset("M_%s" % mech, dest, unreal.Material,
                                      unreal.MaterialFactoryNew())
    if not material:
        raise RuntimeError("[MechMat] could not create material for %s" % mech)
    # Without the usage flag a -game run warns per instance and compiles the
    # skeletal permutation on demand — the overlay material's exact bug.
    material.set_editor_property("used_with_skeletal_mesh", True)
    # Rebuild the graph from empty each run rather than diffing it.
    matlib.delete_all_material_expressions(material)
    sample = matlib.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, -400, 0)
    sample.set_editor_property("texture", texture)
    matlib.connect_material_property(sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    # Fully rough: the pack is flat-painted low-poly and a default specular
    # read makes it look wet under the gym sky.
    rough = matlib.create_material_expression(
        material, unreal.MaterialExpressionConstant, -400, 300)
    rough.set_editor_property("r", 1.0)
    matlib.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    matlib.recompile_material(material)

    mesh = unreal.load_asset("%s/%s" % (dest, mech))
    if not mesh:
        raise RuntimeError("[MechMat] no skeletal mesh at %s/%s" % (dest, mech))
    slots = mesh.get_editor_property("materials")
    rebound = []
    for slot in slots:
        rebound.append(unreal.SkeletalMaterial(
            material_interface=material,
            material_slot_name=slot.get_editor_property("material_slot_name")))
    mesh.set_editor_property("materials", rebound)
    unreal.log("[MechMat] %s: %d slot(s) -> M_%s" % (mech, len(rebound), mech))
    if not unreal.EditorAssetLibrary.save_directory(dest, only_if_is_dirty=False, recursive=True):
        raise RuntimeError("[MechMat] could not save %s" % dest)

unreal.log("[MechMat] DONE.")
