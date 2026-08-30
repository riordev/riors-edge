"""Reports the root-motion flags on every imported mech AnimSequence.

Diagnosis instrument for the stuck-sideways-mech defect, and the reason no
data-side "lock the roots" pass exists: measured 2026-08-30, all 75 mech
sequences ship enable_root_motion=False force_root_lock=False, so nothing
ever moves the NamedBody COMPONENT — the sideways mechs were the death
one-shot's final frame held through a revive path that never re-played the
gait (fixed in ABreakerEnemy). Forcing force_root_lock=True here would pin
the root bone and damage the authored death fall while fixing nothing.
Re-run this after any anim reimport; if a sequence ever reports
enable_root_motion=True, ApplyBodyMesh's rotation reset is the guard that
keeps a revive clean, and THAT sequence deserves a deliberate look.

Run (headless, editor closed):
  UnrealEditor-Cmd.exe <project> -run=pythonscript -script="breaker_report_mech_anim_flags.py"
"""

import unreal

MECHS = ("George", "Leela", "Mike", "Stan")
DEST_ROOT = "/Game/Breaker/Meshes/enemies/mechs"

registry = unreal.AssetRegistryHelpers.get_asset_registry()

for mech in MECHS:
    folder = "%s/%s" % (DEST_ROOT, mech)
    assets = registry.get_assets_by_path(folder, recursive=False)
    if not assets:
        unreal.log_warning("[MechAnimFlags] nothing registered under %s" % folder)
        continue
    for data in assets:
        if data.asset_class_path.asset_name != "AnimSequence":
            continue
        seq = unreal.load_asset(str(data.package_name))
        if not seq:
            unreal.log_warning("[MechAnimFlags] failed to load %s" % data.package_name)
            continue
        unreal.log("[MechAnimFlags] %s enable_root_motion=%s force_root_lock=%s" % (
            data.asset_name,
            seq.get_editor_property("enable_root_motion"),
            seq.get_editor_property("force_root_lock")))

unreal.log("[MechAnimFlags] DONE.")
