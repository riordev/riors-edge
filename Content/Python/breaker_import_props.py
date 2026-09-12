"""Imports the props the runtime spawns on their own (O280).

The props are authored in Scripts/compose_props.py, which sizes each to the
player, grounds it at its own origin (the chest lid at its hinge) and names
it: prop_chest_body, prop_chest_lid, prop_crate4. This script is the other
half: it splits Assets/zones/props.glb into per-name static meshes under
/Game/Breaker/Meshes/props and prints each mesh's imported bounds, so the
sizes the composer printed are CHECKED in engine units here rather than
assumed by the hand that spawns them.

THE FOLDER IS EMPTIED FIRST. A re-import adds and overwrites; it does not
remove, and the Fernhall importer learnt that a re-run over a folder leaves
the previous run's materials and textures standing beside the new ones — a
mesh that then binds to a stale material renders the old sheet. The GLB is
the authority: this folder is its projection, rebuilt from nothing each run.

Run:
  UnrealEditor-Cmd.exe <project> -run=pythonscript -script="breaker_import_props.py"
"""

import os
import unreal

PROJECT_DIR = unreal.SystemLibrary.get_project_directory()
GLB = os.path.normpath(os.path.join(PROJECT_DIR, "Assets", "zones", "props.glb"))
DEST = "/Game/Breaker/Meshes/props"
EXPECTED = ("prop_chest_body", "prop_chest_lid", "prop_crate4")

if not os.path.isfile(GLB):
    raise RuntimeError("[Props] source GLB missing: %s (run Scripts/compose_props.py)" % GLB)

# ---- Empty the folder, so nothing from a previous run survives. -------------
if unreal.EditorAssetLibrary.does_directory_exist(DEST):
    if not unreal.EditorAssetLibrary.delete_directory(DEST):
        raise RuntimeError("[Props] could not delete %s" % DEST)
    unreal.log("[Props] deleted %s" % DEST)

# ---- The meshes. -----------------------------------------------------------
task = unreal.AssetImportTask()
task.filename = GLB
task.destination_path = DEST
task.automated = True
task.replace_existing = True
task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

registry = unreal.AssetRegistryHelpers.get_asset_registry()
assets = registry.get_assets_by_path(DEST, recursive=True)
meshes = {}
for data in assets:
    if data.asset_class_path.asset_name != "StaticMesh":
        continue
    meshes[str(data.asset_name)] = data

missing = [name for name in EXPECTED if name not in meshes]
if missing:
    raise RuntimeError("[Props] export is missing %s (imported: %s)" % (missing, sorted(meshes)))
extra = sorted(name for name in meshes if name not in EXPECTED)
if extra:
    unreal.log_warning("[Props] static meshes the composer does not name: %s" % extra)

for name in sorted(meshes):
    mesh = unreal.load_asset(str(meshes[name].package_name))
    if not mesh:
        raise RuntimeError("[Props] could not load imported mesh %s" % name)
    b = mesh.get_bounds()
    o, e = b.origin, b.box_extent
    unreal.log("[Props] %-16s origin (%7.1f, %7.1f, %7.1f)  extent (%6.1f, %6.1f, %6.1f)  size (%6.1f x %6.1f x %6.1f) cm"
               % (name, o.x, o.y, o.z, e.x, e.y, e.z, e.x * 2, e.y * 2, e.z * 2))

saved = unreal.EditorAssetLibrary.save_directory(DEST, only_if_is_dirty=False, recursive=True)
if not saved:
    raise RuntimeError("[Props] could not save %s" % DEST)
unreal.log("[Props] DONE: %d static meshes in %s (save_directory: %s)" % (len(meshes), DEST, saved))
