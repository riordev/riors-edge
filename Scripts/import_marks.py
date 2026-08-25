# Imports the Fieldplate mark textures: six insignia, five rarity marks.
#
# Run headless from the repo the script sits in:
#   UnrealEditor-Cmd.exe <project> -ExecutePythonScript="Scripts/import_marks.py"
#
# Assets/marks holds the pack's 256px PNGs (stroke-only, #DCE4EE on
# transparency; the SVG sources sit beside them for re-export at other sizes).
# The pack's import settings are law and are applied post-import, because the
# automated factory cannot be told them up front: UserInterface2D compression,
# no mipmaps, sRGB on. Never DXT — the flat panels band immediately. Rarity
# marks TINT to the rarity hex in the widget; insignia never tint.
# Idempotent: re-running reimports in place.

import os
import unreal

SRC = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Assets", "marks"))
DEST = "/Game/Breaker/UI/Marks"

MARKS = {
    "T_InsigniaBreakers": "insignia-breakers_256.png",
    "T_InsigniaCaster": "insignia-caster_256.png",
    "T_InsigniaGunsmith": "insignia-gunsmith_256.png",
    "T_InsigniaSupport": "insignia-support_256.png",
    "T_InsigniaSwift": "insignia-swift_256.png",
    "T_InsigniaTank": "insignia-tank_256.png",
    "T_RarityStandard": "rarity-standard_256.png",
    "T_RarityUncommon": "rarity-uncommon_256.png",
    "T_RarityExceptional": "rarity-exceptional_256.png",
    "T_RarityAberrant": "rarity-aberrant_256.png",
    "T_RarityAnomalous": "rarity-anomalous_256.png",
}

tools = unreal.AssetToolsHelpers.get_asset_tools()

tasks = []
for name, filename in MARKS.items():
    path = os.path.join(SRC, filename)
    if not os.path.isfile(path):
        raise RuntimeError("missing source mark: " + path)
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.save = False  # saved below, after the settings are applied
    task.replace_existing = True
    tasks.append(task)
tools.import_asset_tasks(tasks)

for name in MARKS:
    asset_path = DEST + "/" + name
    texture = unreal.load_asset(asset_path)
    if not texture:
        raise RuntimeError("mark did not import: " + asset_path)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("srgb", True)
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
        raise RuntimeError("could not save " + asset_path)
    unreal.log("imported " + name)

unreal.log("MARKS DONE")
