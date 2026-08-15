"""Creates the three maps the game needs, headlessly.

The project shipped with ONE map (Lvl_FirstPerson), so the title menu, the hub
and the gym were all the same level with a widget drawn over it — which is why
"loading in still takes you to the game": the gym was always already built and
running underneath the menu.

Run via:
  UnrealEditor-Cmd.exe <project> -run=pythonscript -script="breaker_make_levels.py"

Idempotent: a map that already exists is left exactly as it is, so re-running
this never overwrites hand-edited level content.
"""

import unreal

MAPS = [
    # (package path, what it is for)
    ("/Game/Breaker/Maps/Lvl_FrontEnd",
     "Title and character select. Deliberately EMPTY - no game mode content, "
     "nothing spawned, nothing ticking. The point of the split is that loading "
     "the game no longer means building the gym."),
    ("/Game/Breaker/Maps/Lvl_Anchor",
     "The hub. Vendors, the story start, and the navigation gate."),
    ("/Game/Breaker/Maps/Lvl_Gym",
     "The playtest field - waves, F1-F4, the encounter, the TTK instrument."),
]

level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
created = []
skipped = []

for package, purpose in MAPS:
    if unreal.EditorAssetLibrary.does_asset_exist(package):
        skipped.append(package)
        continue
    # new_level saves the current level first and then creates the new one.
    ok = level_sub.new_level(package)
    if ok:
        created.append(package)
    else:
        unreal.log_error("FAILED to create %s" % package)

unreal.log("[BreakerLevels] created: %s" % (created or "none"))
unreal.log("[BreakerLevels] already existed, untouched: %s" % (skipped or "none"))
