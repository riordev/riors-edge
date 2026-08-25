# Imports the Fieldplate font pack's seven faces.
#
# Run headless from the repo the script sits in:
#   UnrealEditor-Cmd.exe <project> -ExecutePythonScript="Scripts/import_fonts.py"
#
# Assets/fonts holds the seven static TTFs (Google Fonts, SIL OFL — the
# licenses sit beside them). This script is the supported-automation route the
# working rules require for binary assets, and it is HALF the job: each TTF
# becomes a UFontFace under /Game/Breaker/UI/Fonts/Faces. The composite half —
# one UFont per role — cannot be scripted from Python (FFontData and
# FTypefaceEntry have no wrappers), so it lives as the editor console command
# BreakerBuildRoleFonts in UI/BreakerFontTools.cpp. Run this, then that.
# Re-running either is idempotent.

import os
import unreal

SRC = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Assets", "fonts"))
DEST = "/Game/Breaker/UI/Fonts"

FACES = {
    "ArchivoSemiBold": "Archivo-SemiBold.ttf",
    "ArchivoBold": "Archivo-Bold.ttf",
    "PlexSansRegular": "IBMPlexSans-Regular.ttf",
    "PlexSansMedium": "IBMPlexSans-Medium.ttf",
    "PlexSansSemiBold": "IBMPlexSans-SemiBold.ttf",
    "PlexMonoRegular": "IBMPlexMono-Regular.ttf",
    "PlexMonoMedium": "IBMPlexMono-Medium.ttf",
}

tools = unreal.AssetToolsHelpers.get_asset_tools()

tasks = []
for name, filename in FACES.items():
    path = os.path.join(SRC, filename)
    if not os.path.isfile(path):
        raise RuntimeError("missing source font: " + path)
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = DEST + "/Faces"
    task.destination_name = "FF_" + name
    task.automated = True
    task.save = True
    task.replace_existing = True
    tasks.append(task)
tools.import_asset_tasks(tasks)

unreal.log("FACES DONE — now run BreakerBuildRoleFonts (UI/BreakerFontTools.cpp) to build the three role fonts")
