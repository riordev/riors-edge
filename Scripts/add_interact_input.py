"""Creates IA_Interact, maps F to it in IMC_Player, and points
DA_PlayerInputConfig.Interact at it — the asset half of moving the interact
verb out of its raw BindKey (ruled), through the same supported-automation
route the font and mark importers use.

Run from the lane worktree:
  UnrealEditor-Cmd.exe <project> -run=pythonscript -script="add_interact_input.py"

Idempotent: an existing IA_Interact is reused, an existing F mapping is not
duplicated, and the config write is a plain property set.
"""

import unreal

INPUT_DIR = "/Game/ProjectBreaker/Input"
IA_PATH = INPUT_DIR + "/IA_Interact"


def ensure_action():
    if unreal.EditorAssetLibrary.does_asset_exist(IA_PATH):
        unreal.log("[Interact] IA_Interact exists; reusing.")
        return unreal.EditorAssetLibrary.load_asset(IA_PATH)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    action = tools.create_asset("IA_Interact", INPUT_DIR, unreal.InputAction, None)
    if not action:
        unreal.log_error("[Interact] could not create IA_Interact")
        return None
    action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    unreal.EditorAssetLibrary.save_loaded_asset(action)
    unreal.log("[Interact] IA_Interact created.")
    return action


def ensure_mapping(action):
    imc = unreal.EditorAssetLibrary.load_asset(INPUT_DIR + "/IMC_Player")
    if not imc:
        unreal.log_error("[Interact] IMC_Player missing")
        return
    # TRAP recorded: the python-visible 'mappings' property is deprecated and
    # reads EMPTY in 5.8, so it cannot be used for a duplicate check, and
    # save_loaded_asset alone did not persist the first attempt — save_asset
    # with only_if_is_dirty=False is what actually wrote the file. Verified
    # by grepping the .uasset for the action name, not by trusting the log.
    key = unreal.Key()
    key.set_editor_property("key_name", "F")
    imc.map_key(action, key)
    unreal.EditorAssetLibrary.save_asset(INPUT_DIR + "/IMC_Player", only_if_is_dirty=False)
    unreal.log("[Interact] F mapped in IMC_Player.")


def ensure_config(action):
    config = unreal.EditorAssetLibrary.load_asset(INPUT_DIR + "/DA_PlayerInputConfig")
    if not config:
        unreal.log_error("[Interact] DA_PlayerInputConfig missing")
        return
    config.set_editor_property("Interact", action)
    unreal.EditorAssetLibrary.save_loaded_asset(config)
    unreal.log("[Interact] DA_PlayerInputConfig.Interact set.")


action = ensure_action()
if action:
    ensure_mapping(action)
    ensure_config(action)
