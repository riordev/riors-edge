import unreal

CONTENT_ROOT = "/Game/ProjectBreaker"
INPUT_ROOT = f"{CONTENT_ROOT}/Input"
CHARACTER_ROOT = f"{CONTENT_ROOT}/Characters"


def require(value, message):
    if value is None:
        raise RuntimeError(message)
    return value


def load_or_create_input_action(name):
    asset_path = f"{INPUT_ROOT}/{name}"
    existing = (unreal.EditorAssetLibrary.load_asset(asset_path)
                if unreal.EditorAssetLibrary.does_asset_exist(asset_path) else None)
    if existing:
        return existing
    factory = unreal.InputAction_Factory()
    return require(
        unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, INPUT_ROOT, unreal.InputAction, factory),
        f"Could not create {asset_path}",
    )


def make_key(key_name):
    key = unreal.Key()
    key.set_editor_property("key_name", key_name)
    return key


def map_keys(context, action, key_names):
    context.unmap_all_keys_from_action(action)
    for key_name in key_names:
        context.map_key(action, make_key(key_name))


def map_additional_keys(context, action, key_names):
    for key_name in key_names:
        key = make_key(key_name)
        context.unmap_key(action, key)
        context.map_key(action, key)


unreal.EditorAssetLibrary.make_directory(INPUT_ROOT)
unreal.EditorAssetLibrary.make_directory(CHARACTER_ROOT)

move = require(unreal.EditorAssetLibrary.load_asset("/Game/Input/Actions/IA_Move"), "Missing IA_Move")
look = require(unreal.EditorAssetLibrary.load_asset("/Game/Input/Actions/IA_Look"), "Missing IA_Look")
jump = require(unreal.EditorAssetLibrary.load_asset("/Game/Input/Actions/IA_Jump"), "Missing IA_Jump")

actions = {
    "sprint": load_or_create_input_action("IA_Sprint"),
    "dash": load_or_create_input_action("IA_Dash"),
    "slide": load_or_create_input_action("IA_Slide"),
    "fire": load_or_create_input_action("IA_Fire"),
    "aim": load_or_create_input_action("IA_Aim"),
    "reload": load_or_create_input_action("IA_Reload"),
    "playtest_reset": load_or_create_input_action("IA_PlaytestReset"),
    "playtest_report": load_or_create_input_action("IA_PlaytestReport"),
    "playtest_diagnostics": load_or_create_input_action("IA_PlaytestDiagnostics"),
    "fov_up": load_or_create_input_action("IA_FOVUp"),
    "fov_down": load_or_create_input_action("IA_FOVDown"),
    "sensitivity_up": load_or_create_input_action("IA_SensitivityUp"),
    "sensitivity_down": load_or_create_input_action("IA_SensitivityDown"),
}

context_path = f"{INPUT_ROOT}/IMC_Player"
context = (unreal.EditorAssetLibrary.load_asset(context_path)
           if unreal.EditorAssetLibrary.does_asset_exist(context_path) else None)
if context is None:
    require(
        unreal.EditorAssetLibrary.duplicate_asset("/Game/Input/IMC_Default", context_path),
        f"Could not duplicate {context_path}",
    )
    context = require(unreal.EditorAssetLibrary.load_asset(context_path), f"Could not load {context_path}")

map_keys(context, actions["sprint"], ["LeftShift", "Gamepad_LeftThumbstick"])
map_keys(context, actions["dash"], ["Q", "Gamepad_FaceButton_Right"])
map_keys(context, actions["slide"], ["C", "LeftControl", "Gamepad_FaceButton_Left"])
map_keys(context, actions["fire"], ["LeftMouseButton", "Gamepad_RightTrigger"])
map_keys(context, actions["aim"], ["RightMouseButton", "Gamepad_LeftTrigger"])
map_keys(context, actions["reload"], ["R", "Gamepad_FaceButton_Top"])
map_keys(context, actions["playtest_reset"], ["F1"])
map_keys(context, actions["playtest_report"], ["F2"])
map_keys(context, actions["playtest_diagnostics"], ["F3"])
map_keys(context, actions["fov_up"], ["RightBracket"])
map_keys(context, actions["fov_down"], ["LeftBracket"])
map_keys(context, actions["sensitivity_up"], ["Equals"])
map_keys(context, actions["sensitivity_down"], ["Hyphen"])
map_additional_keys(context, look, ["Mouse2D"])

config_path = f"{INPUT_ROOT}/DA_PlayerInputConfig"
config = (unreal.EditorAssetLibrary.load_asset(config_path)
          if unreal.EditorAssetLibrary.does_asset_exist(config_path) else None)
if config is None:
    config_class = require(
        unreal.load_class(None, "/Script/RiorsEdge.BreakerInputConfig"),
        "Could not load BreakerInputConfig class",
    )
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", config_class)
    config = require(
        unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "DA_PlayerInputConfig", INPUT_ROOT, None, factory),
        f"Could not create {config_path}",
    )

config.set_editor_property("default_mapping_context", context)
config.set_editor_property("move", move)
config.set_editor_property("look", look)
config.set_editor_property("jump", jump)
for property_name, action in actions.items():
    config.set_editor_property(property_name, action)

blueprint_path = f"{CHARACTER_ROOT}/BP_BreakerCharacter"
blueprint = (unreal.EditorAssetLibrary.load_asset(blueprint_path)
             if unreal.EditorAssetLibrary.does_asset_exist(blueprint_path) else None)
if blueprint is None:
    character_class = require(
        unreal.load_class(None, "/Script/RiorsEdge.BreakerCharacter"),
        "Could not load BreakerCharacter class",
    )
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", character_class)
    blueprint = require(
        unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "BP_BreakerCharacter", CHARACTER_ROOT, unreal.Blueprint, factory),
        f"Could not create {blueprint_path}",
    )

generated_class = require(blueprint.generated_class(), "Blueprint has no generated class")
character_cdo = unreal.get_default_object(generated_class)
character_cdo.set_editor_property("input_config", config)

for asset in [*actions.values(), context, config, blueprint]:
    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)

mapping_data = context.get_editor_property("default_key_mappings")
mapping_count = len(mapping_data.get_editor_property("mappings"))
if mapping_count < 32:
    raise RuntimeError(f"IMC_Player has too few mappings: {mapping_count}")
if character_cdo.get_editor_property("input_config") != config:
    raise RuntimeError("BP_BreakerCharacter did not retain DA_PlayerInputConfig")

unreal.log(f"Rior's Edge: Movement Gym assets ready ({mapping_count} mappings)")
