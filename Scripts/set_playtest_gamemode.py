import unreal

MAP_PATH = "/Game/FirstPerson/Lvl_FirstPerson"
GAME_MODE_PATH = "/Script/RiorsEdge.BreakerGameMode"

world = unreal.EditorAssetLibrary.load_asset(MAP_PATH)
game_mode_class = unreal.load_class(None, GAME_MODE_PATH)

if world is None:
    raise RuntimeError(f"Could not load {MAP_PATH}")
if game_mode_class is None:
    raise RuntimeError(f"Could not load {GAME_MODE_PATH}")

world.get_world_settings().set_editor_property("default_game_mode", game_mode_class)
if not unreal.EditorAssetLibrary.save_loaded_asset(world, only_if_is_dirty=False):
    raise RuntimeError(f"Could not save {MAP_PATH}")

unreal.log(f"Rior's Edge: configured {MAP_PATH} to use {GAME_MODE_PATH}")
