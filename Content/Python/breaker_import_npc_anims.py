"""Imports the Universal Animation Library's clips into each NPC body's
folder, so each import binds to THAT body's skeleton.

The UAL is authored against the same Quaternius universal rig the base
characters ride; importing into the body's own folder lets Interchange find
and reuse the existing skeleton — the same mechanism that refused the
monster imports with "cannot merge bone tree" when rigs DIDN'T match, so a
mismatch here fails loudly rather than binding garbage. Imported once per
body because each body owns its own skeleton asset.

Run (headless, editor closed):
  UnrealEditor-Cmd.exe <project> -run=pythonscript -script="breaker_import_npc_anims.py"
"""

import os
import unreal

PROJECT_DIR = unreal.SystemLibrary.get_project_directory()
UAL = os.path.normpath(os.path.join(PROJECT_DIR, "Assets", "npcs",
    "universal-animation-library-1", "UAL1_Standard.glb"))
if not os.path.isfile(UAL):
    raise RuntimeError("[NPCAnims] missing %s" % UAL)

for body in ("Superhero_Female_FullBody", "Superhero_Male_FullBody"):
    dest = "/Game/Breaker/Meshes/npcs/%s/Anims" % body
    task = unreal.AssetImportTask()
    task.filename = UAL
    task.destination_path = dest
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    anims = [a for a in registry.get_assets_by_path(dest, recursive=True)
             if str(a.asset_class_path.asset_name) == "AnimSequence"]
    unreal.log("[NPCAnims] %s: %d animation sequences." % (body, len(anims)))
    idle = [str(a.asset_name) for a in anims if "idle" in str(a.asset_name).lower()]
    unreal.log("[NPCAnims] %s idle candidates: %s" % (body, ", ".join(sorted(idle)) or "(none)"))

    # The clips import bound to the UAL's OWN skeleton asset, and the body's
    # mesh rides the body's — same bone tree (the universal rig), different
    # assets, and PlayAnimation refuses across assets. The engine's bridge is
    # the compatibility list; declared BOTH ways so neither side's check
    # fails. A genuinely mismatched rig still animates as garbage, which is
    # what the Anchor photograph is for.
    body_skels = [a for a in registry.get_assets_by_path(
        "/Game/Breaker/Meshes/npcs/%s" % body, recursive=True)
        if str(a.asset_class_path.asset_name) == "Skeleton"]
    ual_skel = None
    other = []
    for s in body_skels:
        if "UAL1" in str(s.asset_name):
            ual_skel = s.get_asset()
        else:
            other.append(s.get_asset())
    if ual_skel and other:
        for skel in other:
            for a, b in ((skel, ual_skel), (ual_skel, skel)):
                compat = list(a.get_editor_property("compatible_skeletons"))
                if b not in compat:
                    compat.append(b)
                    a.set_editor_property("compatible_skeletons", compat)
                    unreal.EditorAssetLibrary.save_loaded_asset(a)
        unreal.log("[NPCAnims] %s: skeleton compatibility declared both ways." % body)
    else:
        unreal.log_warning("[NPCAnims] %s: skeleton pair not found (ual=%s, body=%d)."
                           % (body, bool(ual_skel), len(other)))

unreal.log("[NPCAnims] DONE.")
