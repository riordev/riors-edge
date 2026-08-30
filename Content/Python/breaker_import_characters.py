"""Imports the vendored character and enemy placeholder meshes.

The intake branch put CC0 packs under Assets/ (LICENSE-NOTE.txt beside each);
this script is the supported-automation half that turns the curated slice into
uassets. Sources are imported AS THE FILE DICTATES: Quaternius ships its
rigged characters as glTF for Unreal on purpose (the vendored
Unreal-Engine-README records the FBX scaling bug), so rigged sources become
SkeletalMesh and unrigged ones StaticMesh — the consumer's soft-path hook is
expected to accept either and keep its primitive fallback, the same
"floor still works" shape as ABreakerNPC::ApplyBodyMesh.

Scope is deliberately the smallest useful slice: the NPC body family
(universal-base-characters, the owner's one-family cast ruling) and the two
small enemy packs. ultimate-monsters and ultimate-modular-men wait until a
chassis/NPC mapping names which of their fifty-odd meshes the game wants.

Run (headless, editor closed):
  UnrealEditor-Cmd.exe <project> -run=pythonscript -script="breaker_import_characters.py"
"""

import os
import unreal

PROJECT_DIR = unreal.SystemLibrary.get_project_directory()
ASSETS = os.path.normpath(os.path.join(PROJECT_DIR, "Assets"))

# (source file relative to Assets/, destination package path)
# Interchange nests output under a folder named for the source file, which is
# what keeps two packs' identically named textures from colliding.
IMPORTS = []

def add_glob(rel_dir, pattern, dest, per_source_folder=False):
    """per_source_folder gives every file its own destination subfolder. Rigged
    FBX NEED this: ten monsters imported into one folder made Interchange try
    to merge unrelated bone trees into whatever skeleton it found there
    ("cannot merge bone tree with the existing skeleton", ten times), so each
    rig gets a folder no other rig's skeleton can be found in."""
    src_dir = os.path.join(ASSETS, rel_dir)
    if not os.path.isdir(src_dir):
        raise RuntimeError("[Characters] missing source dir: %s" % src_dir)
    found = sorted(n for n in os.listdir(src_dir)
                   if n.lower().endswith(pattern[0]) and n.startswith(pattern[1]))
    if not found:
        raise RuntimeError("[Characters] nothing matching %s in %s" % (pattern, src_dir))
    for name in found:
        target = dest + "/" + os.path.splitext(name)[0] if per_source_folder else dest
        IMPORTS.append((os.path.join(src_dir, name), target))

# The NPC cast: the two rigged bodies, the vendor's Unreal-facing glTF.
# The head-bone hair set joins when an NPC actually wears one.
add_glob(os.path.join("npcs", "universal-base-characters", "BaseCharacters"),
         ((".gltf"), ("Superhero",)), "/Game/Breaker/Meshes/npcs")

# Enemies: the two small packs. Rigged FBX with bare-filename texture
# references that resolve beside the file (checked in the binaries).
add_glob(os.path.join("enemies", "easy-enemy"),
         ((".fbx"), ("",)), "/Game/Breaker/Meshes/enemies")
add_glob(os.path.join("enemies", "sci-fi-essentials"),
         ((".fbx"), ("Enemy_",)), "/Game/Breaker/Meshes/enemies")

# The mechs (George/Leela/Mike/Stan are MACHINE names here — the pack note
# tells the story) and the full monster roster. Each gets its own subfolder
# so fifty-odd flat asset names cannot collide with the small packs'. The
# monsters' FBX reference Atlas_Monsters.png by bare name, so a copy of the
# atlas sits beside each category's files (LFS stores the one content once).
add_glob(os.path.join("enemies", "animated-mech"),
         ((".fbx"), ("",)), "/Game/Breaker/Meshes/enemies/mechs",
         per_source_folder=True)
# The category is part of the destination key: ten monster NAMES exist in
# two categories (Big/Alien and Blob/Alien are different rigs), and a shared
# per-name folder put the second rig onto the first one's skeleton — the same
# merge error, one level down.
for category in ("Big", "Blob", "Flying"):
    add_glob(os.path.join("enemies", "ultimate-monsters", category),
             ((".fbx"), ("",)), "/Game/Breaker/Meshes/enemies/monsters/" + category.lower(),
             per_source_folder=True)

tasks = []
for path, dest in IMPORTS:
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = dest
    task.automated = True
    task.replace_existing = True
    task.save = False  # saved per-directory below
    tasks.append(task)

unreal.log("[Characters] importing %d sources." % len(tasks))
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

# Report what actually landed, by class — the consumer cares whether a body is
# skeletal or static, so the import log states it instead of assuming.
registry = unreal.AssetRegistryHelpers.get_asset_registry()
total = 0
for dest in sorted(set(d for (_, d) in IMPORTS)):
    counts = {}
    for data in registry.get_assets_by_path(dest, recursive=True):
        cls = str(data.asset_class_path.asset_name)
        counts[cls] = counts.get(cls, 0) + 1
    total += sum(counts.values())
    unreal.log("[Characters] %s: %s" % (dest, sorted(counts.items())))
    if not unreal.EditorAssetLibrary.save_directory(dest, only_if_is_dirty=False,
                                                    recursive=True):
        raise RuntimeError("[Characters] could not save %s" % dest)

if total == 0:
    raise RuntimeError("[Characters] nothing imported — every task failed silently.")
unreal.log("[Characters] DONE: %d assets." % total)
