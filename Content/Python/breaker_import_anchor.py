"""Imports the designer's Anchor hub blockout and reports its marker positions.

The model is authored to the spec in Docs/Design/Anchor-Hub-Layout-Brief.md and
carries NAMED objects for the things the code has to agree with — the player
arrival point, both NPCs, and the travel gate. Those names are the contract:
the mesh supplies the space, the code supplies the actors, and they have to
stand in the same places.

OBJ is Y-up; Unreal is Z-up. The importer converts, so the reported positions
below are printed in UNREAL space (X forward, Y right, Z up) to be pasted
straight into BreakerHubBuilder.

Run:
  UnrealEditor-Cmd.exe <project> -run=pythonscript -script="breaker_import_anchor.py"
"""

import os
import unreal

SOURCE = r"C:\Users\rior\Downloads\anchor_hub.glb"
DEST = "/Game/Breaker/Meshes"

# Objects whose positions the C++ has to match. Everything else is dressing.
MARKERS = (
    "player_arrival_capsule",
    "player_arrival_footprint",
    "npc_kess",
    "npc_quartermaster",
    "gate_threshold",
    "gate_post_left",
    "gate_post_right",
)


def import_anchor():
    if not os.path.exists(SOURCE):
        unreal.log_error("[Anchor] source not found: %s" % SOURCE)
        return None

    task = unreal.AssetImportTask()
    task.filename = SOURCE
    task.destination_path = DEST
    task.destination_name = "SM_AnchorHub"
    task.automated = True
    task.replace_existing = True
    task.save = True

    options = unreal.InterchangeGenericAssetsPipeline()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    for path in task.get_editor_property("imported_object_paths") or []:
        unreal.log("[Anchor] imported %s" % path)
    return task.get_editor_property("imported_object_paths")


def report_markers_from_obj():
    """Reads marker centres straight from the OBJ, which is plain text.

    Done from the source rather than from the imported asset because the point
    is to hand the coordinator numbers to put in C++, and the OBJ is the
    authored truth. OBJ is Y-up: Unreal X = obj X, Unreal Y = obj Z,
    Unreal Z = obj Y.
    """
    obj_path = SOURCE.replace(".glb", ".obj")
    if not os.path.exists(obj_path):
        unreal.log_warning("[Anchor] no .obj beside the .glb; skipping marker report")
        return

    current = None
    bounds = {}
    with open(obj_path, "r") as handle:
        for line in handle:
            if line.startswith(("o ", "g ")):
                current = line[2:].strip()
            elif line.startswith("v ") and current in MARKERS:
                _, x, y, z = line.split()[:4]
                x, y, z = float(x), float(y), float(z)
                lo, hi = bounds.get(current, ([x, y, z], [x, y, z]))
                bounds[current] = (
                    [min(lo[0], x), min(lo[1], y), min(lo[2], z)],
                    [max(hi[0], x), max(hi[1], y), max(hi[2], z)],
                )

    unreal.log("[Anchor] marker centres in UNREAL space (X fwd, Y right, Z up):")
    for name in MARKERS:
        if name not in bounds:
            unreal.log_warning("[Anchor]   %-28s MISSING from the model" % name)
            continue
        lo, hi = bounds[name]
        cx = (lo[0] + hi[0]) * 0.5
        cy = (lo[2] + hi[2]) * 0.5   # obj Z -> unreal Y
        cz = (lo[1] + hi[1]) * 0.5   # obj Y -> unreal Z
        unreal.log("[Anchor]   %-28s (%8.1f, %8.1f, %8.1f)  base Z %.1f"
                   % (name, cx, cy, cz, lo[1]))


import_anchor()
report_markers_from_obj()
